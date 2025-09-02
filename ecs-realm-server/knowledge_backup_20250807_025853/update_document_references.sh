#!/bin/bash

# Script to update document number references in all markdown files
# Based on the renumbering mapping provided

set -e

KNOWLEDGE_DIR="/Users/woopinbell/Desktop/tmp/clear-claude-prompt/clear-knowledge/use-portfolio/cpp-multiplayer-game-server/knowledge"

echo "Starting document reference update process..."
echo "Working directory: $KNOWLEDGE_DIR"

# Function to perform sed replacement across all markdown files
update_references() {
    local old_pattern="$1"
    local new_pattern="$2"
    local description="$3"
    
    echo "Updating: $description ($old_pattern -> $new_pattern)"
    
    # Find all markdown files and update them
    find "$KNOWLEDGE_DIR" -name "*.md" -type f -exec sed -i '' "s/${old_pattern}/${new_pattern}/g" {} +
    
    echo "  ✓ Updated $description"
}

# Create backup before making changes
echo "Creating backup..."
backup_dir="${KNOWLEDGE_DIR}_backup_$(date +%Y%m%d_%H%M%S)"
cp -r "$KNOWLEDGE_DIR" "$backup_dir"
echo "  ✓ Backup created at: $backup_dir"

echo ""
echo "Performing reference updates based on mapping:"
echo ""

# Update references based on the provided mapping
# Format: old_number -> new_number

# First, handle the special case for microservices architecture files
update_references "44_microservices_architecture_advanced" "26_microservices_architecture_advanced" "Microservices Advanced Architecture"
update_references "44_microservices_architecture" "25_microservices_architecture" "Microservices Architecture"

# Standard number mappings
update_references "73_" "37_" "Next Gen Game Tech (73 -> 37)"
update_references "66_" "36_" "Realtime Analytics BigData Pipeline (66 -> 36)"
update_references "65_" "35_" "Global Service Operations (65 -> 35)"
update_references "64_" "34_" "Cloud Native Game Server (64 -> 34)"
update_references "63_" "33_" "Kubernetes Operations Guide (63 -> 33)"
update_references "56_" "32_" "Advanced Security Compliance (56 -> 32)"
update_references "55_" "31_" "Security Authentication Guide (55 -> 31)"
update_references "54_" "30_" "Performance Engineering Advanced (54 -> 30)"
update_references "53_" "29_" "Performance Optimization Basics (53 -> 29)"
update_references "46_" "28_" "Event Driven Architecture Guide (46 -> 28)"
update_references "45_" "27_" "gRPC Services Guide (45 -> 27)"
update_references "43_" "24_" "Message Queue Systems (43 -> 24)"
update_references "36_" "23_" "Advanced Database Operations (36 -> 23)"
update_references "35_" "22_" "Redis Cluster Caching Master (35 -> 22)"
update_references "34_" "21_" "NoSQL Integration Guide (34 -> 21)"
update_references "33_" "20_" "Database Design Optimization Guide (33 -> 20)"
update_references "27_" "19_" "AI Game Logic Engine (27 -> 19)"
update_references "26_" "18_" "Realtime Physics Synchronization (26 -> 18)"
update_references "25_" "17_" "System Separation World Management (25 -> 17)"
update_references "24_" "16_" "ECS Architecture System (24 -> 16)"
update_references "23_" "15_" "Game Server Core Concepts (23 -> 15)"
update_references "16_" "14_" "Advanced Networking Optimization (16 -> 14)"
update_references "15_" "13_" "Network Programming Basics (15 -> 13)"
update_references "14_" "12_" "Lockfree Programming Guide (14 -> 12)"
update_references "13_" "11_" "Multithreading Basics (13 -> 11)"

echo ""
echo "Additional pattern updates for consistency:"
echo ""

# Handle any remaining references that might have different patterns
# Update template metaprogramming reference
update_references "33_template_metaprogramming_advanced" "20_template_metaprogramming_advanced" "Template Metaprogramming Advanced"
update_references "33_move_semantics_perfect_forwarding" "20_move_semantics_perfect_forwarding" "Move Semantics Perfect Forwarding"
update_references "33_stl_algorithms_advanced" "20_stl_algorithms_advanced" "STL Algorithms Advanced"

# Handle build systems and testing references
update_references "34_build_systems_advanced" "21_build_systems_advanced" "Build Systems Advanced"
update_references "35_testing_frameworks_complete" "22_testing_frameworks_complete" "Testing Frameworks Complete"
update_references "36_debugging_profiling_toolchain" "23_debugging_profiling_toolchain" "Debugging Profiling Toolchain"

echo ""
echo "Verification: Checking for remaining old references..."
echo ""

# Check for any remaining old references
old_numbers=(13_ 14_ 15_ 16_ 23_ 24_ 25_ 26_ 27_ 33_ 34_ 35_ 36_ 43_ 44_ 45_ 46_ 53_ 54_ 55_ 56_ 63_ 64_ 65_ 66_ 73_)

for num in "${old_numbers[@]}"; do
    count=$(find "$KNOWLEDGE_DIR" -name "*.md" -type f -exec grep -l "${num}" {} + 2>/dev/null | wc -l)
    if [ "$count" -gt 0 ]; then
        echo "⚠️  Still found ${count} files with references to ${num}"
        find "$KNOWLEDGE_DIR" -name "*.md" -type f -exec grep -l "${num}" {} + 2>/dev/null | head -5
    fi
done

echo ""
echo "Update complete!"
echo ""
echo "Summary:"
echo "- Backup created at: $backup_dir"
echo "- Updated document references in all markdown files"
echo "- Checked $(find "$KNOWLEDGE_DIR" -name "*.md" -type f | wc -l) markdown files"
echo ""
echo "To verify the changes, you can run:"
echo "  diff -r '$backup_dir' '$KNOWLEDGE_DIR'"
echo ""
echo "To restore from backup if needed:"
echo "  rm -rf '$KNOWLEDGE_DIR' && mv '$backup_dir' '$KNOWLEDGE_DIR'"