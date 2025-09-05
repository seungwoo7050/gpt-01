# Extension 03: Service SLOs and Alerting

## 1. Objective

With robust metrics being collected from both the server (Ext. 01) and the load generator (Ext. 02), the next logical step is to define what "good performance" means. This guide explains how to define Service Level Objectives (SLOs), implement them as Prometheus alerting rules, and configure Alertmanager to notify you when your service degrades. This moves the project from passive monitoring to proactive alerting.

## 2. Analysis of Current Implementation

Currently, the project has no concept of SLOs or alerting.

*   **Prometheus Metrics**: We have metrics, but no defined thresholds or goals for them.
*   **Alerting Infrastructure**: There is no Prometheus alerting configuration and no Alertmanager setup. We can see when performance is poor by looking at a Grafana dashboard, but the system will not automatically notify us of problems.
*   **Documentation**: There is no `SLO.md` or section in the `README.md` that defines the service's performance targets.

This is not a code-centric extension but rather a critical infrastructure and documentation task.

## 3. Proposed Implementation

### Step 3.1: Defining SLIs and SLOs

First, we must define our **Service Level Indicators (SLIs)**—the quantitative measures of our service's performance—and our **Service Level Objectives (SLOs)**—the target goals for those SLIs.

Let's define two critical SLOs for our game server:

1.  **Availability SLO**: Based on successful logins.
    *   **SLI**: The proportion of successful login requests out of all login attempts.
    *   **SLO**: **99.5%** of login requests over a rolling 28-day period will be successful.

2.  **Latency SLO**: Based on the login-to-response latency we created in Ext. 02.
    *   **SLI**: The proportion of login requests that are processed faster than a given threshold.
    *   **SLO**: **95%** of login requests over a rolling 28-day period will be processed in under **150 milliseconds**.

These SLOs should be documented in a new file, `SLO.md`, at the project root.

**New File (`SLO.md`):**
```markdown
# Service Level Objectives (SLOs)

This document defines the performance and availability targets for the MMORPG server.

## 1. Availability

- **SLI**: `sum(rate(loadtest_connections_total{job="mmorpg_load_test", status="succeeded"}[5m])) / sum(rate(loadtest_connections_total{job="mmorpg_load_test"}[5m]))`
- **SLO**: **99.5%** over a 28-day rolling window.
- **Description**: This measures the percentage of clients that successfully connect and log in to the server during load tests.

## 2. Latency

- **SLI**: `histogram_quantile(0.95, sum(rate(loadtest_login_latency_seconds_bucket{job="mmorpg_load_test"}[5m])) by (le)) < 0.150`
- **SLO**: **95%** of requests will complete in under **150ms** over a 28-day rolling window.
- **Description**: This measures the 95th percentile of the time it takes from a client initiating a connection to receiving a successful login response.

```

### Step 3.2: Creating Prometheus Alerting Rules

Based on these SLOs, we create alerting rules. These are typically stored in a `.yml` file and loaded by Prometheus.

**New File (`prometheus/rules.yml`):**

*(This file should be placed in your Prometheus configuration directory, not the project repository, but is documented here for completeness.)*

```yaml
groups:
- name: MmorpgServerAlerts
  rules:
  - alert: HighLoginLatency
    expr: histogram_quantile(0.95, sum(rate(loadtest_login_latency_seconds_bucket{job="mmorpg_server"}[5m])) by (le)) > 0.150
    for: 5m
    labels:
      severity: page
    annotations:
      summary: "High P95 Login Latency"
      description: "The 95th percentile login latency is {{ $value }}s, which is above the 150ms SLO."

  - alert: LowAvailability
    expr: (sum(rate(loadtest_connections_total{job="mmorpg_server", status="succeeded"}[5m])) / sum(rate(loadtest_connections_total{job="mmorpg_server"}[5m]))) < 0.995
    for: 10m
    labels:
      severity: warning
    annotations:
      summary: "Low Login Success Rate"
      description: "The login success rate is {{ $value | humanizePercentage }}, which is below the 99.5% SLO."

  - alert: ServerIsDown
    expr: up{job="mmorpg_server"} == 0
    for: 1m
    labels:
      severity: critical
    annotations:
      summary: "MMORPG Server is Down"
      description: "The Prometheus target for the MMORPG server has been down for more than 1 minute."
```

### Step 3.3: Setting up Alertmanager

Alertmanager is a separate component that receives alerts from Prometheus and routes them to different receivers like Slack, PagerDuty, or email. A basic configuration to route all alerts to a Slack channel would look like this.

**New File (`alertmanager/config.yml`):**

*(This file is also part of your monitoring infrastructure configuration.)*

```yaml
global:
  resolve_timeout: 5m
  slack_api_url: 'YOUR_SLACK_WEBHOOK_URL'

route:
  group_by: ['alertname', 'job']
  group_wait: 30s
  group_interval: 5m
  repeat_interval: 4h
  receiver: 'slack-notifications'

receivers:
- name: 'slack-notifications'
  slack_configs:
  - channel: '#server-alerts'
    send_resolved: true
    title: '[{{ .Status | toUpper }}{{ if eq .Status "firing" }}:{{ .Alerts.Firing | len }}{{ end }}] {{ .CommonLabels.alertname }}'
    text: "{{ range .Alerts }}
*Alert:* {{ .Annotations.summary }}
*Description:* {{ .Annotations.description }}
{{ end }}"
```
**Action:** You must replace `YOUR_SLACK_WEBHOOK_URL` with a real Slack incoming webhook URL.

## 4. Rationale for Changes

*   **Clarity and Accountability**: Documenting SLOs makes performance targets explicit. The entire team knows what goals they are working towards.
*   **Proactive vs. Reactive**: Alerting turns monitoring from a passive, after-the-fact analysis tool into a proactive system that flags problems as they happen.
*   **Separation of Concerns**: Prometheus is responsible for *generating* alerts based on rules. Alertmanager is responsible for *routing* those alerts (deduplicating, grouping, and sending to the correct channel). This separation makes the system highly flexible.
*   **Severity**: Alerts are given a `severity` label (`warning`, `page`, `critical`). This allows Alertmanager to route critical alerts to on-call engineers via PagerDuty while sending less urgent warnings to a Slack channel.

## 5. Testing and Verification Guide

1.  **Configure Prometheus**: Update your `prometheus.yml` to load the new rule file.
    ```yaml
    rule_files:
      - "rules.yml"
    ```
2.  **Configure and Run Alertmanager**: Run Alertmanager with your new `config.yml`. You can use Docker:
    ```sh
    docker run -d --name alertmanager -p 9093:9093 -v $(pwd)/alertmanager/:/config prom/alertmanager --config.file=/config/config.yml
    ```
3.  **Link Prometheus to Alertmanager**: Add the Alertmanager address to your `prometheus.yml`.
    ```yaml
alerting:
  alertmanagers:
  - static_configs:
    - targets:
      - 'localhost:9093'
```
4.  **Trigger an Alert**: To test the setup, you can create a temporary, easily-triggered alert rule in `rules.yml`.
    *   **Example Test Alert**: `alert: AlwaysFiring expr: vector(1)`. This alert will always be active.
    *   Reload your Prometheus configuration. Within a minute, the alert should fire in Prometheus (visible in the "Alerts" tab of the UI).
    *   Shortly after, you should receive a notification in your configured Slack channel.
5.  **Verify Real Alerts**: Run a load test that intentionally violates an SLO (e.g., by running the server on a very resource-constrained machine to increase latency). Verify that the `HighLoginLatency` alert fires as expected.
6.  **Document**: Add a link to the new `SLO.md` file in your main `README.md`.
