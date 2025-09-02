# 02. Cloud Resources - 클라우드 리소스 프로비저닝 가이드

## 🎯 목표
AWS, GCP, Azure 중 선택한 클라우드 플랫폼에서 게임 서버 운영에 필요한 모든 리소스를 프로비저닝합니다.

## 📋 Prerequisites
- 클라우드 계정 및 결제 설정
- Terraform 0.14+ (Infrastructure as Code)
- 클라우드 CLI 도구 설치
- 예산 승인 및 비용 알림 설정

---

## 1. Terraform을 통한 Infrastructure as Code

### 1.1 프로젝트 구조
```
terraform/
├── environments/
│   ├── dev/
│   │   ├── main.tf
│   │   ├── variables.tf
│   │   └── terraform.tfvars
│   ├── staging/
│   └── production/
├── modules/
│   ├── vpc/
│   ├── compute/
│   ├── database/
│   ├── cache/
│   ├── storage/
│   └── monitoring/
└── global/
    └── backend.tf
```

### 1.2 Backend 설정
**terraform/global/backend.tf**
```hcl
terraform {
  backend "s3" {
    bucket         = "game-server-terraform-state"
    key            = "global/terraform.tfstate"
    region         = "us-east-1"
    encrypt        = true
    dynamodb_table = "terraform-state-lock"
  }
}

# State 버킷 생성
resource "aws_s3_bucket" "terraform_state" {
  bucket = "game-server-terraform-state"
  
  lifecycle {
    prevent_destroy = true
  }
}

resource "aws_s3_bucket_versioning" "terraform_state" {
  bucket = aws_s3_bucket.terraform_state.id
  
  versioning_configuration {
    status = "Enabled"
  }
}

resource "aws_s3_bucket_server_side_encryption_configuration" "terraform_state" {
  bucket = aws_s3_bucket.terraform_state.id
  
  rule {
    apply_server_side_encryption_by_default {
      sse_algorithm = "AES256"
    }
  }
}

# State Lock 테이블
resource "aws_dynamodb_table" "terraform_state_lock" {
  name           = "terraform-state-lock"
  billing_mode   = "PAY_PER_REQUEST"
  hash_key       = "LockID"
  
  attribute {
    name = "LockID"
    type = "S"
  }
}
```

---

## 2. VPC 모듈

### 2.1 VPC 모듈 정의
**terraform/modules/vpc/main.tf**
```hcl
resource "aws_vpc" "main" {
  cidr_block           = var.vpc_cidr
  enable_dns_hostnames = true
  enable_dns_support   = true
  
  tags = {
    Name        = "${var.environment}-game-vpc"
    Environment = var.environment
  }
}

# Public Subnets
resource "aws_subnet" "public" {
  count                   = length(var.availability_zones)
  vpc_id                  = aws_vpc.main.id
  cidr_block              = cidrsubnet(var.vpc_cidr, 8, count.index)
  availability_zone       = var.availability_zones[count.index]
  map_public_ip_on_launch = true
  
  tags = {
    Name = "${var.environment}-public-${count.index + 1}"
    Type = "Public"
  }
}

# Private Subnets for Game Servers
resource "aws_subnet" "private_game" {
  count             = length(var.availability_zones)
  vpc_id            = aws_vpc.main.id
  cidr_block        = cidrsubnet(var.vpc_cidr, 8, count.index + 10)
  availability_zone = var.availability_zones[count.index]
  
  tags = {
    Name = "${var.environment}-private-game-${count.index + 1}"
    Type = "Private"
    Purpose = "GameServers"
  }
}

# Private Subnets for Databases
resource "aws_subnet" "private_db" {
  count             = length(var.availability_zones)
  vpc_id            = aws_vpc.main.id
  cidr_block        = cidrsubnet(var.vpc_cidr, 8, count.index + 20)
  availability_zone = var.availability_zones[count.index]
  
  tags = {
    Name = "${var.environment}-private-db-${count.index + 1}"
    Type = "Private"
    Purpose = "Database"
  }
}

# Internet Gateway
resource "aws_internet_gateway" "main" {
  vpc_id = aws_vpc.main.id
  
  tags = {
    Name = "${var.environment}-igw"
  }
}

# Elastic IPs for NAT Gateways
resource "aws_eip" "nat" {
  count  = var.enable_nat_gateway ? length(var.availability_zones) : 0
  domain = "vpc"
  
  tags = {
    Name = "${var.environment}-eip-${count.index + 1}"
  }
}

# NAT Gateways
resource "aws_nat_gateway" "main" {
  count         = var.enable_nat_gateway ? length(var.availability_zones) : 0
  allocation_id = aws_eip.nat[count.index].id
  subnet_id     = aws_subnet.public[count.index].id
  
  tags = {
    Name = "${var.environment}-nat-${count.index + 1}"
  }
  
  depends_on = [aws_internet_gateway.main]
}

# Route Tables
resource "aws_route_table" "public" {
  vpc_id = aws_vpc.main.id
  
  route {
    cidr_block = "0.0.0.0/0"
    gateway_id = aws_internet_gateway.main.id
  }
  
  tags = {
    Name = "${var.environment}-public-rt"
  }
}

resource "aws_route_table" "private" {
  count  = length(var.availability_zones)
  vpc_id = aws_vpc.main.id
  
  dynamic "route" {
    for_each = var.enable_nat_gateway ? [1] : []
    content {
      cidr_block     = "0.0.0.0/0"
      nat_gateway_id = aws_nat_gateway.main[count.index].id
    }
  }
  
  tags = {
    Name = "${var.environment}-private-rt-${count.index + 1}"
  }
}

# Route Table Associations
resource "aws_route_table_association" "public" {
  count          = length(aws_subnet.public)
  subnet_id      = aws_subnet.public[count.index].id
  route_table_id = aws_route_table.public.id
}

resource "aws_route_table_association" "private_game" {
  count          = length(aws_subnet.private_game)
  subnet_id      = aws_subnet.private_game[count.index].id
  route_table_id = aws_route_table.private[count.index].id
}
```

### 2.2 VPC 모듈 변수
**terraform/modules/vpc/variables.tf**
```hcl
variable "environment" {
  description = "Environment name"
  type        = string
}

variable "vpc_cidr" {
  description = "CIDR block for VPC"
  type        = string
  default     = "10.0.0.0/16"
}

variable "availability_zones" {
  description = "List of availability zones"
  type        = list(string)
  default     = ["us-east-1a", "us-east-1b"]
}

variable "enable_nat_gateway" {
  description = "Enable NAT Gateway for private subnets"
  type        = bool
  default     = true
}
```

---

## 3. 컴퓨팅 리소스 모듈

### 3.1 EKS 클러스터 (Kubernetes)
**terraform/modules/compute/eks.tf**
```hcl
module "eks" {
  source  = "terraform-aws-modules/eks/aws"
  version = "~> 19.0"
  
  cluster_name    = "${var.environment}-game-cluster"
  cluster_version = "1.27"
  
  vpc_id     = var.vpc_id
  subnet_ids = var.private_subnet_ids
  
  # 클러스터 보안 그룹 규칙
  cluster_security_group_additional_rules = {
    ingress_nodes_ephemeral_ports = {
      from_port = 1025
      to_port   = 65535
      protocol  = "tcp"
      type      = "ingress"
      source_node_security_group = true
    }
  }
  
  # 노드 그룹
  eks_managed_node_groups = {
    game_servers = {
      name = "game-server-nodes"
      
      instance_types = ["c5.2xlarge"]
      capacity_type  = "ON_DEMAND"
      
      min_size     = var.min_nodes
      max_size     = var.max_nodes
      desired_size = var.desired_nodes
      
      # 노드 레이블
      labels = {
        Environment = var.environment
        Type        = "game-server"
      }
      
      # 태인트 (전용 노드)
      taints = [
        {
          key    = "dedicated"
          value  = "game-server"
          effect = "NO_SCHEDULE"
        }
      ]
      
      # User data
      pre_bootstrap_user_data = <<-EOT
        #!/bin/bash
        # 커널 파라미터 최적화
        echo "net.core.somaxconn = 65535" >> /etc/sysctl.conf
        echo "net.ipv4.tcp_max_syn_backlog = 65535" >> /etc/sysctl.conf
        sysctl -p
      EOT
      
      # IAM 역할 추가 정책
      iam_role_additional_policies = {
        AmazonSSMManagedInstanceCore = "arn:aws:iam::aws:policy/AmazonSSMManagedInstanceCore"
        CloudWatchAgentServerPolicy  = "arn:aws:iam::aws:policy/CloudWatchAgentServerPolicy"
      }
    }
    
    monitoring = {
      name = "monitoring-nodes"
      
      instance_types = ["t3.large"]
      capacity_type  = "SPOT"
      
      min_size     = 1
      max_size     = 3
      desired_size = 2
      
      labels = {
        Environment = var.environment
        Type        = "monitoring"
      }
    }
  }
  
  # OIDC Provider
  enable_irsa = true
  
  # 애드온
  cluster_addons = {
    coredns = {
      most_recent = true
    }
    kube-proxy = {
      most_recent = true
    }
    vpc-cni = {
      most_recent = true
    }
    aws-ebs-csi-driver = {
      most_recent = true
    }
  }
}

# ALB Controller
resource "helm_release" "aws_load_balancer_controller" {
  name       = "aws-load-balancer-controller"
  repository = "https://aws.github.io/eks-charts"
  chart      = "aws-load-balancer-controller"
  namespace  = "kube-system"
  
  set {
    name  = "clusterName"
    value = module.eks.cluster_name
  }
  
  set {
    name  = "serviceAccount.create"
    value = "true"
  }
  
  set {
    name  = "serviceAccount.annotations.eks\\.amazonaws\\.com/role-arn"
    value = module.aws_load_balancer_controller_irsa.iam_role_arn
  }
}
```

### 3.2 Auto Scaling 설정
**terraform/modules/compute/autoscaling.tf**
```hcl
# Cluster Autoscaler
resource "helm_release" "cluster_autoscaler" {
  name       = "cluster-autoscaler"
  repository = "https://kubernetes.github.io/autoscaler"
  chart      = "cluster-autoscaler"
  namespace  = "kube-system"
  
  values = [
    yamlencode({
      autoDiscovery = {
        clusterName = module.eks.cluster_name
      }
      awsRegion = var.region
      rbac = {
        serviceAccount = {
          annotations = {
            "eks.amazonaws.com/role-arn" = module.cluster_autoscaler_irsa.iam_role_arn
          }
        }
      }
    })
  ]
}

# HPA 설정
resource "kubernetes_horizontal_pod_autoscaler_v2" "game_server" {
  metadata {
    name      = "game-server-hpa"
    namespace = "game-production"
  }
  
  spec {
    scale_target_ref {
      api_version = "apps/v1"
      kind        = "Deployment"
      name        = "game-server"
    }
    
    min_replicas = 3
    max_replicas = 50
    
    metric {
      type = "Resource"
      resource {
        name = "cpu"
        target {
          type                = "Utilization"
          average_utilization = 70
        }
      }
    }
    
    metric {
      type = "Resource"
      resource {
        name = "memory"
        target {
          type                = "Utilization"
          average_utilization = 80
        }
      }
    }
    
    metric {
      type = "Pods"
      pods {
        metric {
          name = "game_active_players"
        }
        target {
          type          = "AverageValue"
          average_value = "1000"
        }
      }
    }
    
    behavior {
      scale_up {
        stabilization_window_seconds = 60
        select_policy                = "Max"
        
        policy {
          type          = "Percent"
          value         = 100
          period_seconds = 60
        }
        
        policy {
          type          = "Pods"
          value         = 10
          period_seconds = 60
        }
      }
      
      scale_down {
        stabilization_window_seconds = 300
        select_policy                = "Min"
        
        policy {
          type          = "Percent"
          value         = 10
          period_seconds = 60
        }
      }
    }
  }
}
```

---

## 4. 데이터베이스 리소스

### 4.1 RDS Aurora 클러스터
**terraform/modules/database/aurora.tf**
```hcl
resource "aws_rds_cluster" "game_db" {
  cluster_identifier     = "${var.environment}-game-aurora"
  engine                 = "aurora-mysql"
  engine_version         = "8.0.mysql_aurora.3.02.0"
  database_name          = "gamedb"
  master_username        = "admin"
  master_password        = random_password.db_password.result
  
  # 백업 설정
  backup_retention_period      = 30
  preferred_backup_window      = "03:00-04:00"
  preferred_maintenance_window = "mon:04:00-mon:05:00"
  
  # 보안
  storage_encrypted               = true
  kms_key_id                     = aws_kms_key.db.arn
  enabled_cloudwatch_logs_exports = ["error", "general", "slowquery"]
  
  # 네트워크
  db_subnet_group_name   = aws_db_subnet_group.main.name
  vpc_security_group_ids = [aws_security_group.db.id]
  
  # 고가용성
  availability_zones = var.availability_zones
  
  # 성능
  db_cluster_parameter_group_name = aws_rds_cluster_parameter_group.game.name
  
  # 스냅샷
  skip_final_snapshot       = false
  final_snapshot_identifier = "${var.environment}-game-aurora-final-${formatdate("YYYYMMDD-hhmmss", timestamp())}"
  
  tags = {
    Name        = "${var.environment}-game-aurora"
    Environment = var.environment
  }
}

# Aurora 인스턴스
resource "aws_rds_cluster_instance" "game_db" {
  count              = var.db_instance_count
  identifier         = "${var.environment}-game-aurora-${count.index + 1}"
  cluster_identifier = aws_rds_cluster.game_db.id
  instance_class     = var.db_instance_class
  engine             = aws_rds_cluster.game_db.engine
  engine_version     = aws_rds_cluster.game_db.engine_version
  
  performance_insights_enabled = true
  monitoring_interval         = 60
  monitoring_role_arn        = aws_iam_role.rds_monitoring.arn
  
  tags = {
    Name = "${var.environment}-game-aurora-${count.index + 1}"
  }
}

# 파라미터 그룹
resource "aws_rds_cluster_parameter_group" "game" {
  name   = "${var.environment}-aurora-mysql8-game"
  family = "aurora-mysql8.0"
  
  parameter {
    name  = "max_connections"
    value = "5000"
  }
  
  parameter {
    name  = "innodb_buffer_pool_size"
    value = "{DBInstanceClassMemory*3/4}"
  }
  
  parameter {
    name  = "slow_query_log"
    value = "1"
  }
  
  parameter {
    name  = "long_query_time"
    value = "2"
  }
}

# Read Replica (Cross-Region)
resource "aws_rds_cluster" "game_db_replica" {
  count                        = var.enable_cross_region_replica ? 1 : 0
  cluster_identifier           = "${var.environment}-game-aurora-replica"
  replication_source_identifier = aws_rds_cluster.game_db.arn
  engine                       = aws_rds_cluster.game_db.engine
  engine_version               = aws_rds_cluster.game_db.engine_version
  
  provider = aws.replica_region
}
```

### 4.2 ElastiCache Redis 클러스터
**terraform/modules/cache/redis.tf**
```hcl
resource "aws_elasticache_replication_group" "game_redis" {
  replication_group_id       = "${var.environment}-game-redis"
  replication_group_description = "Redis cluster for game server"
  
  engine               = "redis"
  engine_version       = "7.0"
  node_type           = var.redis_node_type
  number_cache_clusters = var.redis_cluster_size
  parameter_group_name = aws_elasticache_parameter_group.redis.name
  
  # 자동 페일오버
  automatic_failover_enabled = true
  multi_az_enabled          = true
  
  # 보안
  at_rest_encryption_enabled = true
  transit_encryption_enabled = true
  auth_token                = random_password.redis_auth.result
  
  # 백업
  snapshot_retention_limit = 5
  snapshot_window         = "03:00-05:00"
  
  # 네트워크
  subnet_group_name = aws_elasticache_subnet_group.redis.name
  security_group_ids = [aws_security_group.redis.id]
  
  # 로깅
  log_delivery_configuration {
    destination      = aws_cloudwatch_log_group.redis_slow.name
    destination_type = "cloudwatch-logs"
    log_format       = "json"
    log_type        = "slow-log"
  }
  
  tags = {
    Name        = "${var.environment}-game-redis"
    Environment = var.environment
  }
}

# Redis 파라미터 그룹
resource "aws_elasticache_parameter_group" "redis" {
  name   = "${var.environment}-redis7"
  family = "redis7"
  
  parameter {
    name  = "maxmemory-policy"
    value = "allkeys-lru"
  }
  
  parameter {
    name  = "timeout"
    value = "300"
  }
  
  parameter {
    name  = "tcp-keepalive"
    value = "300"
  }
  
  parameter {
    name  = "maxclients"
    value = "10000"
  }
}
```

---

## 5. 스토리지 리소스

### 5.1 S3 버킷
**terraform/modules/storage/s3.tf**
```hcl
# 게임 자산 버킷
resource "aws_s3_bucket" "game_assets" {
  bucket = "${var.environment}-game-assets-${random_id.bucket_suffix.hex}"
  
  tags = {
    Name        = "${var.environment}-game-assets"
    Environment = var.environment
  }
}

resource "aws_s3_bucket_versioning" "game_assets" {
  bucket = aws_s3_bucket.game_assets.id
  
  versioning_configuration {
    status = "Enabled"
  }
}

resource "aws_s3_bucket_server_side_encryption_configuration" "game_assets" {
  bucket = aws_s3_bucket.game_assets.id
  
  rule {
    apply_server_side_encryption_by_default {
      sse_algorithm = "AES256"
    }
  }
}

# CloudFront 배포
resource "aws_cloudfront_distribution" "game_assets" {
  origin {
    domain_name = aws_s3_bucket.game_assets.bucket_regional_domain_name
    origin_id   = "S3-${aws_s3_bucket.game_assets.id}"
    
    s3_origin_config {
      origin_access_identity = aws_cloudfront_origin_access_identity.game_assets.cloudfront_access_identity_path
    }
  }
  
  enabled             = true
  is_ipv6_enabled    = true
  default_root_object = "index.html"
  
  default_cache_behavior {
    allowed_methods  = ["GET", "HEAD", "OPTIONS"]
    cached_methods   = ["GET", "HEAD"]
    target_origin_id = "S3-${aws_s3_bucket.game_assets.id}"
    
    forwarded_values {
      query_string = false
      cookies {
        forward = "none"
      }
    }
    
    viewer_protocol_policy = "redirect-to-https"
    min_ttl                = 0
    default_ttl            = 86400
    max_ttl                = 31536000
    compress               = true
  }
  
  price_class = "PriceClass_100"
  
  restrictions {
    geo_restriction {
      restriction_type = "none"
    }
  }
  
  viewer_certificate {
    cloudfront_default_certificate = true
  }
  
  tags = {
    Name        = "${var.environment}-game-cdn"
    Environment = var.environment
  }
}

# 백업 버킷
resource "aws_s3_bucket" "backups" {
  bucket = "${var.environment}-game-backups-${random_id.bucket_suffix.hex}"
  
  tags = {
    Name        = "${var.environment}-game-backups"
    Environment = var.environment
  }
}

resource "aws_s3_bucket_lifecycle_configuration" "backups" {
  bucket = aws_s3_bucket.backups.id
  
  rule {
    id     = "delete-old-backups"
    status = "Enabled"
    
    transition {
      days          = 30
      storage_class = "STANDARD_IA"
    }
    
    transition {
      days          = 90
      storage_class = "GLACIER"
    }
    
    expiration {
      days = 365
    }
  }
}
```

---

## 6. 프로덕션 환경 구성

### 6.1 프로덕션 main.tf
**terraform/environments/production/main.tf**
```hcl
terraform {
  required_version = ">= 1.0"
  
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.0"
    }
    kubernetes = {
      source  = "hashicorp/kubernetes"
      version = "~> 2.23"
    }
    helm = {
      source  = "hashicorp/helm"
      version = "~> 2.11"
    }
  }
  
  backend "s3" {
    bucket         = "game-server-terraform-state"
    key            = "production/terraform.tfstate"
    region         = "us-east-1"
    encrypt        = true
    dynamodb_table = "terraform-state-lock"
  }
}

provider "aws" {
  region = var.region
  
  default_tags {
    tags = {
      Environment = "production"
      ManagedBy   = "Terraform"
      Project     = "GameServer"
    }
  }
}

# VPC
module "vpc" {
  source = "../../modules/vpc"
  
  environment        = "production"
  vpc_cidr          = "10.0.0.0/16"
  availability_zones = ["us-east-1a", "us-east-1b", "us-east-1c"]
  enable_nat_gateway = true
}

# EKS Cluster
module "compute" {
  source = "../../modules/compute"
  
  environment         = "production"
  vpc_id             = module.vpc.vpc_id
  private_subnet_ids = module.vpc.private_subnet_ids
  
  min_nodes     = 3
  max_nodes     = 20
  desired_nodes = 5
}

# Database
module "database" {
  source = "../../modules/database"
  
  environment         = "production"
  vpc_id             = module.vpc.vpc_id
  database_subnet_ids = module.vpc.database_subnet_ids
  
  db_instance_class = "db.r5.2xlarge"
  db_instance_count = 3
  enable_cross_region_replica = true
}

# Cache
module "cache" {
  source = "../../modules/cache"
  
  environment      = "production"
  vpc_id          = module.vpc.vpc_id
  cache_subnet_ids = module.vpc.cache_subnet_ids
  
  redis_node_type    = "cache.r6g.xlarge"
  redis_cluster_size = 3
}

# Storage
module "storage" {
  source = "../../modules/storage"
  
  environment = "production"
}

# Monitoring
module "monitoring" {
  source = "../../modules/monitoring"
  
  environment = "production"
  vpc_id      = module.vpc.vpc_id
}
```

### 6.2 비용 최적화
**terraform/modules/compute/spot_instances.tf**
```hcl
# Spot 인스턴스 사용 (비 크리티컬 워크로드)
resource "aws_eks_node_group" "spot" {
  cluster_name    = module.eks.cluster_name
  node_group_name = "spot-nodes"
  node_role_arn   = aws_iam_role.node_group.arn
  subnet_ids      = var.private_subnet_ids
  
  capacity_type = "SPOT"
  
  instance_types = [
    "c5.xlarge",
    "c5a.xlarge",
    "c5n.xlarge",
    "c6i.xlarge"
  ]
  
  scaling_config {
    desired_size = 2
    max_size     = 10
    min_size     = 0
  }
  
  # Spot 인스턴스 다양성
  launch_template {
    id      = aws_launch_template.spot.id
    version = "$Latest"
  }
  
  labels = {
    lifecycle = "spot"
    workload  = "batch"
  }
  
  taints = [
    {
      key    = "spot"
      value  = "true"
      effect = "NO_SCHEDULE"
    }
  ]
}

# Reserved Instances 구매 추천
data "aws_ce_cost_and_usage" "monthly" {
  time_period {
    start = formatdate("YYYY-MM-DD", timeadd(timestamp(), "-30d"))
    end   = formatdate("YYYY-MM-DD", timestamp())
  }
  
  granularity = "MONTHLY"
  metrics     = ["UnblendedCost"]
  
  group_by {
    type = "DIMENSION"
    key  = "INSTANCE_TYPE"
  }
}

output "cost_optimization_recommendations" {
  value = {
    current_monthly_cost = data.aws_ce_cost_and_usage.monthly.results
    recommendation = "Consider purchasing Reserved Instances for consistent workloads"
  }
}
```

---

## 7. 배포 명령어

### 7.1 초기 설정
```bash
# Terraform 초기화
cd terraform/environments/production
terraform init

# 실행 계획 확인
terraform plan -out=tfplan

# 리소스 생성
terraform apply tfplan
```

### 7.2 업데이트 및 관리
```bash
# 변경사항 확인
terraform plan

# 특정 모듈만 업데이트
terraform apply -target=module.compute

# 리소스 상태 확인
terraform show

# 비용 예측
terraform plan -out=tfplan
terraform show -json tfplan | jq '.resource_changes[] | select(.change.actions[] == "create") | .type'
```

---

## ✅ 체크리스트

### 필수 확인사항
- [ ] Terraform state 백엔드 구성
- [ ] VPC 및 서브넷 생성
- [ ] EKS 클러스터 배포
- [ ] RDS Aurora 클러스터 생성
- [ ] ElastiCache Redis 구성
- [ ] S3 버킷 및 CloudFront 설정
- [ ] IAM 역할 및 정책 생성
- [ ] 보안 그룹 규칙 검증

### 권장사항
- [ ] 비용 태그 전략 수립
- [ ] Reserved Instances 구매 검토
- [ ] Spot 인스턴스 활용
- [ ] 자동 백업 정책 설정
- [ ] 리소스 모니터링 대시보드

## 🎯 다음 단계
→ [04_docker_registry.md](04_docker_registry.md) - Docker 레지스트리 설정