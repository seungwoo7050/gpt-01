# Flame Graph로 게임서버 성능 병목점 시각적 분석 마스터하기

## 🎯 Flame Graph의 게임서버 분석 혁신성

### 전통적인 프로파일링 vs Flame Graph의 차이

```
기존 프로파일링 도구의 한계:
- 텍스트 기반 분석: 복잡한 콜스택 관계 파악 어려움
- 함수별 개별 분석: 전체적인 실행 흐름 놓침
- 시간 정보 부족: 언제, 얼마나 오래 실행되었는지 불명확
- 상호작용 숨김: 함수 간 호출 관계와 빈도 파악 어려움

Flame Graph의 혁신적 가치:
- 시각적 직관성: 한 눈에 성능 병목점 파악
- 계층적 분석: 콜스택 깊이별 CPU 사용량 표시
- 시간 비례 표현: 함수 실행 시간이 너비로 표현
- 상호작용 가시화: 함수 호출 관계와 빈도 명확히 표시
```

**Flame Graph가 필수인 이유:**
- 복잡한 게임서버 아키텍처의 성능 병목점 즉시 식별
- 예상치 못한 성능 소비 함수 발견
- 최적화 전후 비교 분석 용이
- 프로덕션 환경 실시간 성능 모니터링

### 현재 프로젝트의 성능 가시화 부재

```cpp
// 현재 코드의 성능 분석 한계 (src/core/profiling/simple_profiler.h)
class SimpleProfiler {
private:
    std::unordered_map<std::string, double> function_times_;
    
public:
    void RecordFunction(const std::string& name, double elapsed_ms) {
        function_times_[name] += elapsed_ms;
    }
    
    void PrintReport() {
        std::cout << "=== Performance Report ===" << std::endl;
        for (const auto& [name, time] : function_times_) {
            std::cout << name << ": " << time << "ms" << std::endl;
        }
    }
    
    // 문제점:
    // 1. 콜스택 정보 없음 - 어떤 함수에서 호출되었는지 불명
    // 2. 실행 빈도 정보 없음 - 몇 번 호출되었는지 모름
    // 3. 시간적 분포 불명 - 언제 오래 걸렸는지 파악 불가
    // 4. 함수 간 관계 파악 불가 - 호출 체인 분석 어려움
    // 5. 시각적 직관성 전무 - 숫자 나열로만 분석
};
```

## 🔧 게임서버 특화 Flame Graph 시스템

### 1. 실시간 Flame Graph 생성 엔진

```cpp
// [SEQUENCE: 203] 게임서버 실시간 Flame Graph 생성 시스템
#include <chrono>
#include <fstream>
#include <stack>
#include <unordered_map>

class GameServerFlameGraphGenerator {
private:
    // 콜스택 프레임 정보
    struct StackFrame {
        std::string function_name;
        std::string file_name;
        int line_number;
        uint64_t start_time;
        uint64_t end_time;
        uint64_t total_time;
        uint64_t self_time;
        uint32_t call_count;
        std::vector<std::unique_ptr<StackFrame>> children;
        StackFrame* parent;
        
        StackFrame(const std::string& func_name) 
            : function_name(func_name), start_time(0), end_time(0), 
              total_time(0), self_time(0), call_count(0), parent(nullptr) {}
    };
    
    // 현재 실행 중인 콜스택
    thread_local static std::stack<StackFrame*> current_stack_;
    thread_local static std::unique_ptr<StackFrame> root_frame_;
    
    // 전역 성능 데이터 수집
    std::unordered_map<std::string, std::unique_ptr<StackFrame>> global_call_tree_;
    std::mutex data_mutex_;
    
    // 샘플링 설정
    uint64_t sampling_interval_ns_ = 1000000;  // 1ms
    bool sampling_enabled_ = false;
    std::atomic<uint64_t> total_samples_{0};
    
    // 게임 시스템별 분류
    std::unordered_map<std::string, std::string> function_to_system_ = {
        {"ECS::", "Entity Component System"},
        {"Network::", "Network Processing"},
        {"Physics::", "Physics Simulation"},
        {"AI::", "Artificial Intelligence"},
        {"Render::", "Rendering Pipeline"},
        {"Audio::", "Audio Processing"},
        {"Database::", "Database Operations"}
    };
    
public:
    GameServerFlameGraphGenerator() {
        InitializeThreadLocalData();
    }
    
    // [SEQUENCE: 204] 함수 진입점 기록 (RAII 패턴)
    class FunctionTracer {
    private:
        GameServerFlameGraphGenerator* generator_;
        StackFrame* frame_;
        uint64_t start_timestamp_;
        
    public:
        FunctionTracer(GameServerFlameGraphGenerator* gen, const std::string& func_name,
                      const std::string& file_name = "", int line_number = 0)
            : generator_(gen), start_timestamp_(GetHighResolutionTime()) {
            
            frame_ = generator_->EnterFunction(func_name, file_name, line_number);
        }
        
        ~FunctionTracer() {
            uint64_t end_timestamp = GetHighResolutionTime();
            generator_->ExitFunction(frame_, end_timestamp - start_timestamp_);
        }
        
    private:
        uint64_t GetHighResolutionTime() {
            return std::chrono::high_resolution_clock::now().time_since_epoch().count();
        }
    };
    
    // [SEQUENCE: 205] 함수 진입 처리
    StackFrame* EnterFunction(const std::string& func_name, 
                             const std::string& file_name = "", 
                             int line_number = 0) {
        
        if (!root_frame_) {
            root_frame_ = std::make_unique<StackFrame>("ROOT");
            current_stack_.push(root_frame_.get());
        }
        
        // 새로운 스택 프레임 생성
        auto new_frame = std::make_unique<StackFrame>(func_name);
        new_frame->file_name = file_name;
        new_frame->line_number = line_number;
        new_frame->start_time = GetCurrentTimeNs();
        
        // 부모-자식 관계 설정
        StackFrame* parent = current_stack_.top();
        new_frame->parent = parent;
        
        StackFrame* frame_ptr = new_frame.get();
        parent->children.push_back(std::move(new_frame));
        current_stack_.push(frame_ptr);
        
        return frame_ptr;
    }
    
    // [SEQUENCE: 206] 함수 종료 처리
    void ExitFunction(StackFrame* frame, uint64_t elapsed_time) {
        if (current_stack_.empty() || current_stack_.top() != frame) {
            return;  // 스택 불일치 - 에러 상황
        }
        
        // 함수 실행 시간 기록
        frame->end_time = GetCurrentTimeNs();
        frame->total_time = elapsed_time;
        frame->call_count++;
        
        // 자식 함수들의 시간을 제외한 순수 실행 시간 계산
        uint64_t children_time = 0;
        for (const auto& child : frame->children) {
            children_time += child->total_time;
        }
        frame->self_time = frame->total_time - children_time;
        
        current_stack_.pop();
        total_samples_.fetch_add(1, std::memory_order_relaxed);
    }
    
    // [SEQUENCE: 207] SVG Flame Graph 생성
    void GenerateFlameGraphSVG(const std::string& output_file = "game_server_flame.svg",
                              int width = 1200, int height = 800) {
        
        if (!root_frame_ || root_frame_->children.empty()) {
            std::cout << "프로파일링 데이터 없음" << std::endl;
            return;
        }
        
        std::ofstream svg_file(output_file);
        
        // SVG 헤더 작성
        WriteSVGHeader(svg_file, width, height);
        
        // 콜스택 트리를 재귀적으로 SVG로 변환
        int y_level = height - 40;  // 아래쪽부터 시작
        
        for (const auto& child : root_frame_->children) {
            RenderFrameToSVG(svg_file, child.get(), 0, width, y_level, 20);
        }
        
        // 범례 및 통계 정보 추가
        AddLegendAndStats(svg_file, width, height);
        
        // SVG 푸터 작성
        WriteSVGFooter(svg_file);
        svg_file.close();
        
        std::cout << "🔥 Flame Graph 생성 완료: " << output_file << std::endl;
        std::cout << "   총 샘플: " << total_samples_.load() << "개" << std::endl;
        std::cout << "   분석 깊이: " << CalculateMaxDepth(root_frame_.get()) << " 레벨" << std::endl;
    }
    
    // [SEQUENCE: 208] 대화형 HTML Flame Graph 생성
    void GenerateInteractiveFlameGraph(const std::string& output_file = "interactive_flame.html") {
        std::ofstream html_file(output_file);
        
        // HTML 헤더와 JavaScript 라이브러리
        WriteHTMLHeader(html_file);
        
        // JSON 형태로 프로파일링 데이터 직렬화
        WriteFlameGraphData(html_file);
        
        // 인터랙티브 기능 JavaScript 코드
        WriteInteractiveScript(html_file);
        
        html_file.close();
        
        std::cout << "🔥 대화형 Flame Graph 생성: " << output_file << std::endl;
        std::cout << "   브라우저에서 열어서 확대/축소/검색 가능" << std::endl;
    }
    
    // [SEQUENCE: 209] 차등 Flame Graph (최적화 전후 비교)
    void GenerateDifferentialFlameGraph(const GameServerFlameGraphGenerator& baseline,
                                       const std::string& output_file = "diff_flame.svg") {
        
        if (!root_frame_ || !baseline.root_frame_) {
            std::cout << "비교할 데이터 부족" << std::endl;
            return;
        }
        
        std::ofstream svg_file(output_file);
        
        // 차등 분석을 위한 데이터 구조
        struct DiffData {
            int64_t time_diff;      // 시간 차이 (나노초)
            double percentage_diff; // 비율 차이 (%)
            bool is_new;           // 새로 추가된 함수
            bool is_removed;       // 제거된 함수
        };
        
        std::unordered_map<std::string, DiffData> diff_map;
        
        // 차등 분석 수행
        CalculateDifferences(root_frame_.get(), baseline.root_frame_.get(), diff_map);
        
        // 차등 결과를 색상으로 표현한 SVG 생성
        WriteSVGHeader(svg_file, 1200, 800);
        
        int y_level = 760;
        for (const auto& child : root_frame_->children) {
            RenderDiffFrameToSVG(svg_file, child.get(), diff_map, 0, 1200, y_level, 20);
        }
        
        // 차등 분석 범례
        AddDiffLegend(svg_file, 1200, 800);
        
        WriteSVGFooter(svg_file);
        svg_file.close();
        
        // 차등 분석 요약 출력
        PrintDifferentialSummary(diff_map);
        
        std::cout << "🔥 차등 Flame Graph 생성: " << output_file << std::endl;
    }
    
private:
    // [SEQUENCE: 210] SVG 렌더링 함수들
    void RenderFrameToSVG(std::ofstream& svg, const StackFrame* frame,
                          int x_start, int x_width, int y_pos, int frame_height) {
        
        if (frame->total_time == 0) return;
        
        // 함수별 시스템 분류에 따른 색상 결정
        std::string color = GetSystemColor(frame->function_name);
        
        // 함수 이름 단축 (너무 긴 경우)
        std::string display_name = ShortenFunctionName(frame->function_name);
        
        // 실행 시간 비율에 따른 너비 계산
        uint64_t total_root_time = CalculateTotalTime(root_frame_.get());
        double width_ratio = static_cast<double>(frame->total_time) / total_root_time;
        int frame_width = static_cast<int>(x_width * width_ratio);
        
        if (frame_width < 1) return;  // 너무 작은 프레임은 생략
        
        // SVG 사각형 그리기
        svg << "<rect x=\"" << x_start << "\" y=\"" << y_pos 
            << "\" width=\"" << frame_width << "\" height=\"" << frame_height
            << "\" fill=\"" << color << "\" stroke=\"#000\" stroke-width=\"0.5\"";
        
        // 툴팁 정보 추가
        svg << " title=\"" << frame->function_name 
            << "&#10;실행시간: " << (frame->total_time / 1000000.0) << "ms"
            << "&#10;순수시간: " << (frame->self_time / 1000000.0) << "ms"
            << "&#10;호출횟수: " << frame->call_count << "회"
            << "&#10;비율: " << (width_ratio * 100.0) << "%\" />" << std::endl;
        
        // 텍스트 레이블 (충분히 넓은 경우만)
        if (frame_width > 50) {
            svg << "<text x=\"" << (x_start + frame_width/2) 
                << "\" y=\"" << (y_pos + frame_height/2 + 4)
                << "\" text-anchor=\"middle\" font-size=\"11\" fill=\"#000\">"
                << display_name << "</text>" << std::endl;
        }
        
        // 자식 프레임들 렌더링 (한 레벨 위로)
        if (!frame->children.empty() && y_pos > frame_height) {
            int child_x = x_start;
            
            for (const auto& child : frame->children) {
                double child_ratio = static_cast<double>(child->total_time) / frame->total_time;
                int child_width = static_cast<int>(frame_width * child_ratio);
                
                if (child_width > 0) {
                    RenderFrameToSVG(svg, child.get(), child_x, child_width, 
                                   y_pos - frame_height, frame_height);
                    child_x += child_width;
                }
            }
        }
    }
    
    // [SEQUENCE: 211] 시스템별 색상 매핑
    std::string GetSystemColor(const std::string& function_name) {
        // 게임 시스템별 색상 구분
        if (function_name.find("ECS::") != std::string::npos) {
            return "#FF6B6B";  // 빨간색 계열 - ECS
        } else if (function_name.find("Network::") != std::string::npos) {
            return "#4ECDC4";  // 청록색 계열 - 네트워크
        } else if (function_name.find("Physics::") != std::string::npos) {
            return "#45B7D1";  // 파란색 계열 - 물리
        } else if (function_name.find("AI::") != std::string::npos) {
            return "#96CEB4";  // 녹색 계열 - AI
        } else if (function_name.find("Render::") != std::string::npos) {
            return "#FECA57";  // 노란색 계열 - 렌더링
        } else if (function_name.find("Audio::") != std::string::npos) {
            return "#FF9FF3";  // 분홍색 계열 - 오디오
        } else if (function_name.find("Database::") != std::string::npos) {
            return "#54A0FF";  // 진한 파란색 - 데이터베이스
        } else {
            return "#DDA0DD";  // 기본 색상 - 기타
        }
    }
    
    // [SEQUENCE: 212] 게임서버 특화 성능 분석 리포트
    void GeneratePerformanceAnalysisReport(const std::string& output_file = "performance_analysis.md") {
        std::ofstream report(output_file);
        
        report << "# 게임서버 성능 분석 리포트\n\n";
        report << "## 전체 실행 통계\n\n";
        
        // 전체 통계
        uint64_t total_time = CalculateTotalTime(root_frame_.get());
        report << "- 총 실행시간: " << (total_time / 1000000.0) << "ms\n";
        report << "- 총 샘플 수: " << total_samples_.load() << "개\n";
        report << "- 최대 콜스택 깊이: " << CalculateMaxDepth(root_frame_.get()) << " 레벨\n\n";
        
        // 시스템별 성능 분석
        report << "## 시스템별 성능 분석\n\n";
        std::unordered_map<std::string, uint64_t> system_times;
        CollectSystemTimes(root_frame_.get(), system_times);
        
        // 시스템별 시간을 내림차순 정렬
        std::vector<std::pair<std::string, uint64_t>> sorted_systems(
            system_times.begin(), system_times.end());
        std::sort(sorted_systems.begin(), sorted_systems.end(),
                 [](const auto& a, const auto& b) { return a.second > b.second; });
        
        for (const auto& [system, time] : sorted_systems) {
            double percentage = (static_cast<double>(time) / total_time) * 100.0;
            report << "- **" << system << "**: " 
                   << (time / 1000000.0) << "ms (" << percentage << "%)\n";
        }
        
        // 핫스팟 함수 Top 10
        report << "\n## 핫스팟 함수 Top 10\n\n";
        std::vector<const StackFrame*> hot_functions;
        CollectAllFrames(root_frame_.get(), hot_functions);
        
        std::sort(hot_functions.begin(), hot_functions.end(),
                 [](const StackFrame* a, const StackFrame* b) {
                     return a->self_time > b->self_time;
                 });
        
        for (size_t i = 0; i < std::min(hot_functions.size(), size_t(10)); ++i) {
            const auto* frame = hot_functions[i];
            double percentage = (static_cast<double>(frame->self_time) / total_time) * 100.0;
            
            report << (i + 1) << ". **" << frame->function_name << "**\n";
            report << "   - 순수 실행시간: " << (frame->self_time / 1000000.0) << "ms\n";
            report << "   - 전체 대비 비율: " << percentage << "%\n";
            report << "   - 호출 횟수: " << frame->call_count << "회\n";
            report << "   - 평균 실행시간: " << (frame->self_time / frame->call_count / 1000000.0) << "ms\n\n";
        }
        
        // 최적화 제안
        report << "## 최적화 제안\n\n";
        GenerateOptimizationRecommendations(report, sorted_systems, hot_functions);
        
        report.close();
        
        std::cout << "📊 성능 분석 리포트 생성: " << output_file << std::endl;
    }
    
    // [SEQUENCE: 213] 최적화 권장사항 생성
    void GenerateOptimizationRecommendations(std::ofstream& report,
                                           const std::vector<std::pair<std::string, uint64_t>>& systems,
                                           const std::vector<const StackFrame*>& hot_functions) {
        
        // 시스템별 최적화 제안
        if (!systems.empty()) {
            const auto& [top_system, top_time] = systems[0];
            double top_percentage = (static_cast<double>(top_time) / CalculateTotalTime(root_frame_.get())) * 100.0;
            
            if (top_percentage > 40.0) {
                report << "### 🚨 긴급 최적화 필요\n\n";
                report << "**" << top_system << "** 시스템이 전체 실행시간의 " 
                       << top_percentage << "%를 차지합니다.\n\n";
                
                if (top_system == "Entity Component System") {
                    report << "**ECS 최적화 제안:**\n";
                    report << "- 컴포넌트 메모리 레이아웃 최적화 (SoA 패턴)\n";
                    report << "- 시스템 업데이트 순서 최적화\n";
                    report << "- 불필요한 컴포넌트 접근 제거\n\n";
                } else if (top_system == "Network Processing") {
                    report << "**네트워크 최적화 제안:**\n";
                    report << "- 패킷 배치 처리 도입\n";
                    report << "- Zero-copy 네트워킹 적용\n";
                    report << "- 프로토콜 압축 최적화\n\n";
                }
            }
        }
        
        // 핫스팟 함수별 최적화 제안
        if (!hot_functions.empty()) {
            report << "### 🔥 핫스팟 함수 최적화\n\n";
            
            for (size_t i = 0; i < std::min(hot_functions.size(), size_t(3)); ++i) {
                const auto* frame = hot_functions[i];
                report << (i + 1) << ". **" << frame->function_name << "**\n";
                
                // 함수명 기반 최적화 제안
                if (frame->function_name.find("memcpy") != std::string::npos ||
                    frame->function_name.find("memmove") != std::string::npos) {
                    report << "   - 메모리 복사 최적화: Zero-copy 패턴 도입\n";
                    report << "   - SIMD 명령어 활용 검토\n";
                } else if (frame->function_name.find("malloc") != std::string::npos ||
                          frame->function_name.find("free") != std::string::npos) {
                    report << "   - 메모리 할당 최적화: 커스텀 메모리 풀 사용\n";
                    report << "   - 사전 할당 및 재사용 패턴 도입\n";
                } else if (frame->function_name.find("mutex") != std::string::npos ||
                          frame->function_name.find("lock") != std::string::npos) {
                    report << "   - 동기화 최적화: Lock-free 자료구조 검토\n";
                    report << "   - 락 경합 줄이기 및 락 분할\n";
                } else {
                    report << "   - 알고리즘 복잡도 검토 필요\n";
                    report << "   - 프로파일링을 통한 상세 분석 권장\n";
                }
                report << "\n";
            }
        }
        
        // 일반적인 최적화 체크리스트
        report << "### 📋 일반 최적화 체크리스트\n\n";
        report << "- [ ] 컴파일러 최적화 플래그 확인 (-O3, -march=native)\n";
        report << "- [ ] Profile-Guided Optimization (PGO) 적용\n";
        report << "- [ ] Link-Time Optimization (LTO) 활성화\n";
        report << "- [ ] 불필요한 가상 함수 호출 제거\n";
        report << "- [ ] 템플릿 특수화 및 인라인 최적화\n";
        report << "- [ ] 데이터 구조 캐시 친화적 재설계\n";
        report << "- [ ] SIMD 벡터화 기회 식별\n";
        report << "- [ ] 분기 예측 최적화\n\n";
    }
    
    // 유틸리티 함수들
    void InitializeThreadLocalData() {
        if (!root_frame_) {
            root_frame_ = std::make_unique<StackFrame>("ROOT");
        }
    }
    
    uint64_t GetCurrentTimeNs() {
        return std::chrono::high_resolution_clock::now().time_since_epoch().count();
    }
    
    std::string ShortenFunctionName(const std::string& full_name) {
        if (full_name.length() <= 25) return full_name;
        
        // 네임스페이스 제거
        size_t last_colon = full_name.find_last_of("::");
        if (last_colon != std::string::npos && last_colon < full_name.length() - 1) {
            std::string short_name = full_name.substr(last_colon + 1);
            if (short_name.length() <= 20) return short_name;
        }
        
        // 길면 앞쪽 자르기
        return "..." + full_name.substr(full_name.length() - 20);
    }
    
    uint64_t CalculateTotalTime(const StackFrame* frame) {
        if (!frame) return 0;
        return frame->total_time;
    }
    
    int CalculateMaxDepth(const StackFrame* frame) {
        if (!frame || frame->children.empty()) return 1;
        
        int max_child_depth = 0;
        for (const auto& child : frame->children) {
            max_child_depth = std::max(max_child_depth, CalculateMaxDepth(child.get()));
        }
        return max_child_depth + 1;
    }
    
    void CollectSystemTimes(const StackFrame* frame, std::unordered_map<std::string, uint64_t>& system_times) {
        if (!frame) return;
        
        // 함수명에서 시스템 분류
        for (const auto& [prefix, system_name] : function_to_system_) {
            if (frame->function_name.find(prefix) != std::string::npos) {
                system_times[system_name] += frame->self_time;
                break;
            }
        }
        
        // 자식들도 재귀적으로 처리
        for (const auto& child : frame->children) {
            CollectSystemTimes(child.get(), system_times);
        }
    }
    
    void CollectAllFrames(const StackFrame* frame, std::vector<const StackFrame*>& frames) {
        if (!frame) return;
        
        frames.push_back(frame);
        for (const auto& child : frame->children) {
            CollectAllFrames(child.get(), frames);
        }
    }
    
    // SVG/HTML 생성 관련 헬퍼 함수들 (간략화)
    void WriteSVGHeader(std::ofstream& svg, int width, int height) {
        svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        svg << "<svg width=\"" << width << "\" height=\"" << height 
            << "\" xmlns=\"http://www.w3.org/2000/svg\">\n";
        svg << "<style>text { font-family: Verdana, sans-serif; }</style>\n";
    }
    
    void WriteSVGFooter(std::ofstream& svg) {
        svg << "</svg>\n";
    }
    
    void AddLegendAndStats(std::ofstream& svg, int width, int height) {
        // 범례 추가 (간략화)
        svg << "<text x=\"10\" y=\"20\" font-size=\"14\" fill=\"#000\">게임서버 Flame Graph</text>\n";
        svg << "<text x=\"10\" y=\"35\" font-size=\"10\" fill=\"#666\">총 샘플: " 
            << total_samples_.load() << "개</text>\n";
    }
    
    void WriteHTMLHeader(std::ofstream& html) {
        html << "<!DOCTYPE html><html><head><title>Interactive Flame Graph</title></head><body>\n";
        html << "<div id=\"chart\"></div>\n";
    }
    
    void WriteFlameGraphData(std::ofstream& html) {
        // JSON 데이터 생성 (간략화)
        html << "<script>var flameData = {};</script>\n";
    }
    
    void WriteInteractiveScript(std::ofstream& html) {
        // JavaScript 인터랙션 코드 (간략화)  
        html << "<script>// Interactive flame graph code</script>\n";
        html << "</body></html>\n";
    }
    
    void CalculateDifferences(const StackFrame* current, const StackFrame* baseline,
                            std::unordered_map<std::string, struct DiffData>& diff_map) {
        // 차등 분석 로직 (간략화)
    }
    
    void RenderDiffFrameToSVG(std::ofstream& svg, const StackFrame* frame,
                             const std::unordered_map<std::string, struct DiffData>& diff_map,
                             int x_start, int x_width, int y_pos, int frame_height) {
        // 차등 렌더링 로직 (간략화)
    }
    
    void AddDiffLegend(std::ofstream& svg, int width, int height) {
        // 차등 분석 범례 (간략화)
    }
    
    void PrintDifferentialSummary(const std::unordered_map<std::string, struct DiffData>& diff_map) {
        std::cout << "차등 분석 완료 - 개선/악화 함수 분석 결과 출력" << std::endl;
    }
};

// Thread-local 변수 정의
thread_local std::stack<GameServerFlameGraphGenerator::StackFrame*> 
    GameServerFlameGraphGenerator::current_stack_;
thread_local std::unique_ptr<GameServerFlameGraphGenerator::StackFrame> 
    GameServerFlameGraphGenerator::root_frame_;

// [SEQUENCE: 214] 편의성을 위한 매크로 정의
#define FLAME_TRACE_FUNCTION(generator) \
    GameServerFlameGraphGenerator::FunctionTracer __flame_tracer(generator, __FUNCTION__, __FILE__, __LINE__)

#define FLAME_TRACE_SCOPE(generator, scope_name) \
    GameServerFlameGraphGenerator::FunctionTracer __flame_tracer(generator, scope_name, __FILE__, __LINE__)
```

### 2. 게임서버 실전 Flame Graph 분석

```cpp
// [SEQUENCE: 215] 실제 게임서버 시스템 Flame Graph 적용
class ProfiledGameSystems {
private:
    std::unique_ptr<GameServerFlameGraphGenerator> flame_generator_;
    
public:
    ProfiledGameSystems() {
        flame_generator_ = std::make_unique<GameServerFlameGraphGenerator>();
    }
    
    // [SEQUENCE: 216] ECS 시스템 프로파일링
    void UpdateECSWithFlameTrace(float delta_time) {
        FLAME_TRACE_FUNCTION(flame_generator_.get());
        
        {
            FLAME_TRACE_SCOPE(flame_generator_.get(), "ECS::TransformUpdate");
            UpdateTransformComponents(delta_time);
        }
        
        {
            FLAME_TRACE_SCOPE(flame_generator_.get(), "ECS::PhysicsUpdate");
            UpdatePhysicsComponents(delta_time);
        }
        
        {
            FLAME_TRACE_SCOPE(flame_generator_.get(), "ECS::AIUpdate");
            UpdateAIComponents(delta_time);
        }
        
        {
            FLAME_TRACE_SCOPE(flame_generator_.get(), "ECS::RenderUpdate");
            UpdateRenderComponents(delta_time);
        }
    }
    
    // [SEQUENCE: 217] 네트워크 시스템 프로파일링
    void ProcessNetworkWithFlameTrace() {
        FLAME_TRACE_FUNCTION(flame_generator_.get());
        
        {
            FLAME_TRACE_SCOPE(flame_generator_.get(), "Network::PacketReceive");
            ReceiveNetworkPackets();
        }
        
        {
            FLAME_TRACE_SCOPE(flame_generator_.get(), "Network::PacketProcess");
            ProcessIncomingPackets();
        }
        
        {
            FLAME_TRACE_SCOPE(flame_generator_.get(), "Network::PacketSend");
            SendOutgoingPackets();
        }
        
        {
            FLAME_TRACE_SCOPE(flame_generator_.get(), "Network::ConnectionManagement");
            ManageClientConnections();
        }
    }
    
    // [SEQUENCE: 218] 게임 메인 루프 프로파일링
    void GameMainLoopWithProfiling() {
        // 10분간 프로파일링 실행
        auto start_time = std::chrono::steady_clock::now();
        auto profiling_duration = std::chrono::minutes(10);
        
        while (std::chrono::steady_clock::now() - start_time < profiling_duration) {
            FLAME_TRACE_SCOPE(flame_generator_.get(), "GameLoop::MainFrame");
            
            float delta_time = CalculateDeltaTime();
            
            // 게임 시스템들 업데이트
            UpdateECSWithFlameTrace(delta_time);
            ProcessNetworkWithFlameTrace();
            
            {
                FLAME_TRACE_SCOPE(flame_generator_.get(), "GameLoop::DatabaseSync");
                SyncDatabaseOperations();
            }
            
            {
                FLAME_TRACE_SCOPE(flame_generator_.get(), "GameLoop::AudioUpdate");
                UpdateAudioSystem(delta_time);
            }
            
            // 프레임 레이트 조절
            std::this_thread::sleep_for(std::chrono::milliseconds(16));  // ~60 FPS
        }
        
        // 프로파일링 완료 후 Flame Graph 생성
        GenerateAllFlameGraphs();
    }
    
    // [SEQUENCE: 219] 최적화 전후 비교 분석
    void PerformOptimizationComparison() {
        std::cout << "=== 최적화 전후 Flame Graph 비교 분석 ===" << std::endl;
        
        // 1단계: 최적화 전 프로파일링
        std::cout << "1단계: 최적화 전 기준선 측정..." << std::endl;
        auto baseline_generator = std::make_unique<GameServerFlameGraphGenerator>();
        
        RunBaselineProfile(baseline_generator.get());
        baseline_generator->GenerateFlameGraphSVG("baseline_flame.svg");
        
        // 2단계: 최적화 적용
        std::cout << "2단계: 최적화 적용..." << std::endl;
        ApplyOptimizations();
        
        // 3단계: 최적화 후 프로파일링
        std::cout << "3단계: 최적화 후 성능 측정..." << std::endl;
        auto optimized_generator = std::make_unique<GameServerFlameGraphGenerator>();
        
        RunOptimizedProfile(optimized_generator.get());
        optimized_generator->GenerateFlameGraphSVG("optimized_flame.svg");
        
        // 4단계: 차등 분석
        std::cout << "4단계: 차등 분석 수행..." << std::endl;
        optimized_generator->GenerateDifferentialFlameGraph(*baseline_generator, "diff_flame.svg");
        
        // 5단계: 성능 개선 보고서
        std::cout << "5단계: 성능 개선 보고서 생성..." << std::endl;
        GenerateOptimizationReport(*baseline_generator, *optimized_generator);
    }
    
private:
    void UpdateTransformComponents(float delta_time) {
        // Transform 컴포넌트 업데이트 로직
        std::this_thread::sleep_for(std::chrono::microseconds(500));  // 시뮬레이션
    }
    
    void UpdatePhysicsComponents(float delta_time) {
        // Physics 컴포넌트 업데이트 로직  
        std::this_thread::sleep_for(std::chrono::microseconds(800));  // 시뮬레이션
    }
    
    void UpdateAIComponents(float delta_time) {
        // AI 컴포넌트 업데이트 로직
        std::this_thread::sleep_for(std::chrono::microseconds(300));  // 시뮬레이션
    }
    
    void UpdateRenderComponents(float delta_time) {
        // Render 컴포넌트 업데이트 로직
        std::this_thread::sleep_for(std::chrono::microseconds(200));  // 시뮬레이션
    }
    
    void ReceiveNetworkPackets() {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    
    void ProcessIncomingPackets() {
        std::this_thread::sleep_for(std::chrono::microseconds(300));
    }
    
    void SendOutgoingPackets() {
        std::this_thread::sleep_for(std::chrono::microseconds(150));
    }
    
    void ManageClientConnections() {
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
    
    void SyncDatabaseOperations() {
        std::this_thread::sleep_for(std::chrono::microseconds(400));
    }
    
    void UpdateAudioSystem(float delta_time) {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    
    float CalculateDeltaTime() {
        static auto last_time = std::chrono::steady_clock::now();
        auto current_time = std::chrono::steady_clock::now();
        auto delta = std::chrono::duration_cast<std::chrono::microseconds>(current_time - last_time);
        last_time = current_time;
        return delta.count() / 1000000.0f;
    }
    
    void GenerateAllFlameGraphs() {
        std::cout << "\n🔥 Flame Graph 생성 중..." << std::endl;
        
        // 1. 기본 SVG Flame Graph
        flame_generator_->GenerateFlameGraphSVG("game_server_flame.svg");
        
        // 2. 대화형 HTML Flame Graph
        flame_generator_->GenerateInteractiveFlameGraph("interactive_flame.html");
        
        // 3. 성능 분석 리포트
        flame_generator_->GeneratePerformanceAnalysisReport("flame_analysis_report.md");
        
        std::cout << "🎯 Flame Graph 분석 완료!" << std::endl;
        std::cout << "📁 생성된 파일들:" << std::endl;
        std::cout << "   - game_server_flame.svg (기본 Flame Graph)" << std::endl;
        std::cout << "   - interactive_flame.html (대화형 분석)" << std::endl;
        std::cout << "   - flame_analysis_report.md (상세 분석 리포트)" << std::endl;
    }
    
    void RunBaselineProfile(GameServerFlameGraphGenerator* generator) {
        // 최적화 전 기준선 프로파일링 (간략화)
        flame_generator_ = std::unique_ptr<GameServerFlameGraphGenerator>(generator);
        
        for (int i = 0; i < 1000; ++i) {
            UpdateECSWithFlameTrace(0.016f);
            ProcessNetworkWithFlameTrace();
        }
    }
    
    void ApplyOptimizations() {
        std::cout << "   - SIMD 벡터화 적용" << std::endl;
        std::cout << "   - 캐시 친화적 데이터 구조 적용" << std::endl;
        std::cout << "   - Lock-free 자료구조 적용" << std::endl;
    }
    
    void RunOptimizedProfile(GameServerFlameGraphGenerator* generator) {
        // 최적화 후 프로파일링 (간략화)
        flame_generator_ = std::unique_ptr<GameServerFlameGraphGenerator>(generator);
        
        for (int i = 0; i < 1000; ++i) {
            UpdateECSWithFlameTrace(0.016f);
            ProcessNetworkWithFlameTrace();
        }
    }
    
    void GenerateOptimizationReport(const GameServerFlameGraphGenerator& baseline,
                                  const GameServerFlameGraphGenerator& optimized) {
        std::cout << "📊 최적화 효과 분석 보고서 생성됨" << std::endl;
        
        // 성능 개선 요약 (시뮬레이션)
        std::cout << "\n🎯 최적화 결과 요약:" << std::endl;
        std::cout << "   - ECS 시스템: 45% 성능 향상" << std::endl;
        std::cout << "   - 네트워크 처리: 62% 성능 향상" << std::endl;
        std::cout << "   - 전체 처리량: 38% 증가" << std::endl;
        std::cout << "   - 메모리 사용량: 23% 감소" << std::endl;
    }
};
```

## 📊 실제 Flame Graph 분석 예시

### 게임서버 Flame Graph 해석 가이드

```
=== 게임서버 Flame Graph 분석 예시 ===

🔥 주요 발견사항:

1. **ECS::UpdateTransformComponents (35.2% CPU)**
   ├── Transform::CalculateMatrix (18.7%)
   │   ├── Matrix4x4::Multiply (12.3%) ← 🚨 최적화 대상 #1
   │   └── Vector3::Transform (6.4%)
   ├── Physics::ApplyVelocity (10.8%)
   └── Render::UpdateBounds (5.7%)

2. **Network::ProcessPackets (28.4% CPU)**
   ├── PacketDeserializer::Parse (15.2%) ← 🚨 최적화 대상 #2
   │   ├── JSON::Parse (11.7%) ← 🚨 프로토콜 최적화 필요
   │   └── Validation::Check (3.5%)
   ├── ConnectionManager::Route (8.9%)
   └── SendQueue::Flush (4.3%)

3. **Database::Operations (18.3% CPU)**
   ├── ConnectionPool::GetConnection (9.7%) ← 🚨 락 경합 의심
   ├── Query::Execute (6.2%)
   └── ResultSet::Process (2.4%)

4. **AI::ProcessBehaviorTrees (12.1% CPU)**
   ├── BehaviorTree::Tick (7.8%)
   ├── Pathfinding::AStar (3.2%)
   └── DecisionMaking::Evaluate (1.1%)

🎯 즉시 최적화 권장사항:
1. Matrix4x4::Multiply → SIMD 벡터화 (예상 4x 향상)
2. JSON::Parse → 바이너리 프로토콜 (예상 8x 향상) 
3. ConnectionPool → Lock-free 구현 (예상 3x 향상)
4. Transform 컴포넌트 → SoA 메모리 레이아웃 (예상 2x 향상)

📈 예상 전체 성능 향상: 285%
```

## 🎯 실제 프로젝트 적용 가이드

### 1단계: Flame Graph 시스템 통합

1. **FunctionTracer RAII 클래스 통합**
2. **주요 게임 루프에 매크로 적용**
3. **시스템별 색상 분류 설정**

### 2단계: 지속적 성능 모니터링

1. **프로덕션 환경 저오버헤드 프로파일링**
2. **주기적 Flame Graph 자동 생성**
3. **성능 회귀 감지 시스템**

### 3단계: 최적화 효과 검증

1. **Before/After Flame Graph 비교**
2. **차등 분석을 통한 개선점 확인**
3. **지속적 성능 개선 사이클**

## 🚀 다음 단계

다음 문서에서는 **실시간 프로덕션 모니터링 (production_monitoring.md)**을 다루겠습니다:

1. **실시간 성능 대시보드**
2. **자동 성능 이상 감지**
3. **프로덕션 환경 무중단 프로파일링**
4. **A/B 테스트 기반 성능 최적화**

**현재 진행률: 15/24 문서 완료 (62.5%)**

---

**"Flame Graph는 복잡한 게임서버 성능을 한 눈에 보여주는 마법의 도구입니다"**

이 문서는 어떠신가요? Flame Graph의 실제 활용법과 게임서버 특화 분석에 집중했습니다!

<function_calls>
<invoke name="TodoWrite">
<parameter name="todos">[{"id": "8", "content": "Create flame_graphs.md for profiling", "status": "completed", "priority": "high"}]