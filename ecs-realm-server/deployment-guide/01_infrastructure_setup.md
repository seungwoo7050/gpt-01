# 01. Infrastructure Setup - 인프라 구축 가이드

## 🎯 목표
게임 서버 운영을 위한 기본 인프라를 구축하고, 네트워크 및 보안 설정을 완료합니다.

## 📋 Prerequisites
- AWS/GCP/Azure 계정
- 도메인 이름 (예: mmorpg-game.com)
- SSH 키 페어 생성
- 예산 승인 (월 $2,000-3,000)

---

## 1. 클라우드 제공자 선택

### 1.1 AWS 추천 구성
```yaml
Region: us-east-1 (또는 ap-northeast-2 for Korea)
Services:
  - EC2: 게임 서버 인스턴스
  - RDS: MySQL 데이터베이스
  - ElastiCache: Redis 캐싱
  - ALB: 로드 밸런싱
  - S3: 정적 자산 저장
  - CloudWatch: 모니터링
```

### 1.2 네트워크 아키텍처
```
Internet
    │
    ├── CloudFront (CDN)
    │
    ├── Route 53 (DNS)
    │
    └── VPC (10.0.0.0/16)
        │
        ├── Public Subnet (10.0.1.0/24)
        │   ├── NAT Gateway
        │   └── Application Load Balancer
        │
        ├── Private Subnet A (10.0.10.0/24)
        │   ├── Game Server Zone 1
        │   └── Game Server Zone 2
        │
        ├── Private Subnet B (10.0.20.0/24)
        │   ├── RDS Primary
        │   └── RDS Standby
        │
        └── Private Subnet C (10.0.30.0/24)
            └── ElastiCache Redis Cluster
```

---

## 2. VPC 및 네트워크 설정

### 2.1 VPC 생성
```bash
# AWS CLI를 사용한 VPC 생성
aws ec2 create-vpc \
    --cidr-block 10.0.0.0/16 \
    --tag-specifications 'ResourceType=vpc,Tags=[{Key=Name,Value=GameServerVPC}]'

# VPC ID 저장
export VPC_ID=vpc-xxxxx
```

### 2.2 서브넷 생성
```bash
# Public Subnet
aws ec2 create-subnet \
    --vpc-id $VPC_ID \
    --cidr-block 10.0.1.0/24 \
    --availability-zone us-east-1a \
    --tag-specifications 'ResourceType=subnet,Tags=[{Key=Name,Value=PublicSubnet}]'

# Private Subnet A (Game Servers)
aws ec2 create-subnet \
    --vpc-id $VPC_ID \
    --cidr-block 10.0.10.0/24 \
    --availability-zone us-east-1a \
    --tag-specifications 'ResourceType=subnet,Tags=[{Key=Name,Value=PrivateSubnetA}]'

# Private Subnet B (Database)
aws ec2 create-subnet \
    --vpc-id $VPC_ID \
    --cidr-block 10.0.20.0/24 \
    --availability-zone us-east-1b \
    --tag-specifications 'ResourceType=subnet,Tags=[{Key=Name,Value=PrivateSubnetB}]'
```

### 2.3 Internet Gateway 및 NAT Gateway
```bash
# Internet Gateway 생성 및 연결
aws ec2 create-internet-gateway \
    --tag-specifications 'ResourceType=internet-gateway,Tags=[{Key=Name,Value=GameServerIGW}]'

aws ec2 attach-internet-gateway \
    --internet-gateway-id igw-xxxxx \
    --vpc-id $VPC_ID

# Elastic IP 할당
aws ec2 allocate-address --domain vpc

# NAT Gateway 생성
aws ec2 create-nat-gateway \
    --subnet-id subnet-public-xxxxx \
    --allocation-id eipalloc-xxxxx \
    --tag-specifications 'ResourceType=nat-gateway,Tags=[{Key=Name,Value=GameServerNAT}]'
```

### 2.4 라우팅 테이블 설정
```bash
# Public 라우팅 테이블
aws ec2 create-route \
    --route-table-id rtb-public-xxxxx \
    --destination-cidr-block 0.0.0.0/0 \
    --gateway-id igw-xxxxx

# Private 라우팅 테이블
aws ec2 create-route \
    --route-table-id rtb-private-xxxxx \
    --destination-cidr-block 0.0.0.0/0 \
    --nat-gateway-id nat-xxxxx
```

---

## 3. 보안 그룹 설정

### 3.1 게임 서버 보안 그룹
```bash
# 보안 그룹 생성
aws ec2 create-security-group \
    --group-name GameServerSG \
    --description "Security group for game servers" \
    --vpc-id $VPC_ID

# 인바운드 규칙 추가
aws ec2 authorize-security-group-ingress \
    --group-id sg-gameserver \
    --protocol tcp \
    --port 8080 \
    --source-group sg-alb  # ALB에서만 접근 허용

aws ec2 authorize-security-group-ingress \
    --group-id sg-gameserver \
    --protocol tcp \
    --port 22 \
    --cidr 10.0.0.0/16  # VPC 내부에서만 SSH
```

### 3.2 데이터베이스 보안 그룹
```bash
# RDS 보안 그룹
aws ec2 create-security-group \
    --group-name DatabaseSG \
    --description "Security group for RDS" \
    --vpc-id $VPC_ID

aws ec2 authorize-security-group-ingress \
    --group-id sg-database \
    --protocol tcp \
    --port 3306 \
    --source-group sg-gameserver  # 게임 서버에서만 접근
```

### 3.3 Redis 보안 그룹
```bash
# ElastiCache 보안 그룹
aws ec2 create-security-group \
    --group-name RedisSG \
    --description "Security group for Redis" \
    --vpc-id $VPC_ID

aws ec2 authorize-security-group-ingress \
    --group-id sg-redis \
    --protocol tcp \
    --port 6379 \
    --source-group sg-gameserver
```

---

## 4. 로드 밸런서 설정

### 4.1 Application Load Balancer 생성
```bash
# ALB 생성
aws elbv2 create-load-balancer \
    --name game-server-alb \
    --subnets subnet-public-1 subnet-public-2 \
    --security-groups sg-alb \
    --scheme internet-facing \
    --type application \
    --ip-address-type ipv4
```

### 4.2 타겟 그룹 생성
```bash
# 게임 서버 타겟 그룹
aws elbv2 create-target-group \
    --name game-servers-tg \
    --protocol HTTP \
    --port 8080 \
    --vpc-id $VPC_ID \
    --health-check-enabled \
    --health-check-path /health \
    --health-check-interval-seconds 30 \
    --health-check-timeout-seconds 5 \
    --healthy-threshold-count 2 \
    --unhealthy-threshold-count 3
```

### 4.3 리스너 규칙 설정
```bash
# HTTP 리스너 (나중에 HTTPS로 리다이렉트)
aws elbv2 create-listener \
    --load-balancer-arn arn:aws:elasticloadbalancing:... \
    --protocol HTTP \
    --port 80 \
    --default-actions Type=redirect,RedirectConfig='{Protocol=HTTPS,Port=443,StatusCode=HTTP_301}'

# HTTPS 리스너 (SSL 인증서 필요)
aws elbv2 create-listener \
    --load-balancer-arn arn:aws:elasticloadbalancing:... \
    --protocol HTTPS \
    --port 443 \
    --certificates CertificateArn=arn:aws:acm:... \
    --default-actions Type=forward,TargetGroupArn=arn:aws:elasticloadbalancing:...
```

---

## 5. 컴퓨팅 리소스 준비

### 5.1 EC2 인스턴스 시작 템플릿
```bash
# 시작 템플릿 생성
aws ec2 create-launch-template \
    --launch-template-name GameServerTemplate \
    --launch-template-data '{
        "ImageId": "ami-0c55b159cbfafe1f0",
        "InstanceType": "c5.2xlarge",
        "KeyName": "game-server-key",
        "SecurityGroupIds": ["sg-gameserver"],
        "IamInstanceProfile": {
            "Name": "GameServerRole"
        },
        "BlockDeviceMappings": [{
            "DeviceName": "/dev/xvda",
            "Ebs": {
                "VolumeSize": 100,
                "VolumeType": "gp3",
                "DeleteOnTermination": true
            }
        }],
        "UserData": "#!/bin/bash\n# 초기화 스크립트"
    }'
```

### 5.2 Auto Scaling Group 설정
```bash
# Auto Scaling Group 생성
aws autoscaling create-auto-scaling-group \
    --auto-scaling-group-name game-server-asg \
    --launch-template LaunchTemplateName=GameServerTemplate,Version='$Latest' \
    --min-size 2 \
    --max-size 10 \
    --desired-capacity 4 \
    --vpc-zone-identifier "subnet-private-a,subnet-private-b" \
    --target-group-arns arn:aws:elasticloadbalancing:... \
    --health-check-type ELB \
    --health-check-grace-period 300
```

---

## 6. 데이터베이스 설정

### 6.1 RDS MySQL 인스턴스
```bash
# DB 서브넷 그룹 생성
aws rds create-db-subnet-group \
    --db-subnet-group-name game-db-subnet \
    --db-subnet-group-description "Subnet group for game database" \
    --subnet-ids subnet-private-b subnet-private-c

# RDS 인스턴스 생성
aws rds create-db-instance \
    --db-instance-identifier game-mysql-prod \
    --db-instance-class db.r5.xlarge \
    --engine mysql \
    --engine-version 8.0.35 \
    --master-username admin \
    --master-user-password $DB_PASSWORD \
    --allocated-storage 100 \
    --storage-type gp3 \
    --storage-encrypted \
    --vpc-security-group-ids sg-database \
    --db-subnet-group-name game-db-subnet \
    --backup-retention-period 7 \
    --preferred-backup-window "03:00-04:00" \
    --preferred-maintenance-window "Mon:04:00-Mon:05:00" \
    --multi-az \
    --auto-minor-version-upgrade
```

### 6.2 ElastiCache Redis 클러스터
```bash
# Redis 서브넷 그룹
aws elasticache create-cache-subnet-group \
    --cache-subnet-group-name game-redis-subnet \
    --cache-subnet-group-description "Subnet group for Redis" \
    --subnet-ids subnet-private-c

# Redis 클러스터 생성
aws elasticache create-replication-group \
    --replication-group-id game-redis-cluster \
    --replication-group-description "Redis cluster for game server" \
    --cache-node-type cache.r5.large \
    --engine redis \
    --engine-version 7.0 \
    --num-cache-clusters 3 \
    --automatic-failover-enabled \
    --cache-subnet-group-name game-redis-subnet \
    --security-group-ids sg-redis \
    --at-rest-encryption-enabled \
    --transit-encryption-enabled \
    --snapshot-retention-limit 5
```

---

## 7. 스토리지 설정

### 7.1 S3 버킷 생성
```bash
# 게임 자산 버킷
aws s3api create-bucket \
    --bucket game-assets-prod \
    --region us-east-1 \
    --acl private

# 백업 버킷
aws s3api create-bucket \
    --bucket game-backups-prod \
    --region us-east-1 \
    --acl private

# 버전 관리 활성화
aws s3api put-bucket-versioning \
    --bucket game-assets-prod \
    --versioning-configuration Status=Enabled

# 수명 주기 정책
aws s3api put-bucket-lifecycle-configuration \
    --bucket game-backups-prod \
    --lifecycle-configuration file://lifecycle.json
```

### 7.2 EBS 스냅샷 정책
```bash
# DLM(Data Lifecycle Manager) 정책 생성
aws dlm create-lifecycle-policy \
    --execution-role-arn arn:aws:iam::... \
    --description "Daily snapshots for game servers" \
    --state ENABLED \
    --policy-details file://snapshot-policy.json
```

---

## 8. IAM 역할 및 정책

### 8.1 EC2 인스턴스 역할
```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": [
        "s3:GetObject",
        "s3:PutObject"
      ],
      "Resource": "arn:aws:s3:::game-assets-prod/*"
    },
    {
      "Effect": "Allow",
      "Action": [
        "secretsmanager:GetSecretValue"
      ],
      "Resource": "arn:aws:secretsmanager:*:*:secret:game-*"
    },
    {
      "Effect": "Allow",
      "Action": [
        "cloudwatch:PutMetricData",
        "logs:CreateLogGroup",
        "logs:CreateLogStream",
        "logs:PutLogEvents"
      ],
      "Resource": "*"
    }
  ]
}
```

---

## ✅ 체크리스트

### 필수 확인사항
- [ ] VPC 및 서브넷 생성 완료
- [ ] 보안 그룹 규칙 설정 완료
- [ ] NAT Gateway 정상 작동
- [ ] Load Balancer 생성 및 설정
- [ ] RDS 인스턴스 실행 중
- [ ] Redis 클러스터 실행 중
- [ ] S3 버킷 생성 완료
- [ ] IAM 역할 생성 및 연결

### 권장사항
- [ ] CloudWatch 대시보드 생성
- [ ] 비용 알림 설정
- [ ] AWS Config 활성화
- [ ] CloudTrail 로깅 활성화

## 🎯 다음 단계
→ [02_cloud_resources.md](02_cloud_resources.md) - 클라우드 리소스 상세 설정