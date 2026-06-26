# Mini DCS Architecture & Application（迷你 DCS 架构与应用）

**从零开始、零依赖的 C 语言实现**，涵盖工业分布式控制系统（DCS）架构、现场总线协议、报警管理标准以及商用 DCS 平台模型。每个模块对应 MIT、Stanford、CMU 和 RWTH Aachen 等顶尖高校课程，将工业自动化理论与可运行的 C 代码相结合。

## 模块总览

| 模块 | 主题 | 参考课程 |
|-----------|--------|-------------|
| [mini-dcs-alarm-management-isa182](mini-dcs-alarm-management-isa182/) | ISA-18.2 报警生命周期、死区检测、报警合理化、KPI 指标、审计追踪 | MIT 6.302, ISA-18.2 / IEC 62682 |
| [mini-dcs-redundancy-failover](mini-dcs-redundancy-failover/) | 控制器/网络冗余、故障切换状态机、可用性建模、诊断监测、状态同步 | MIT 6.302, CMU 24-677, RWTH Aachen |
| [mini-dcs-system-architecture](mini-dcs-system-architecture/) | ISA-95 层级映射、DCS 报警管理、控制器冗余、安全仪表系统集成（IEC 61508/61511）、系统配置数据库 | MIT 2.171, ISA-95, IEC 61508 |
| [mini-emerson-deltav-system](mini-emerson-deltav-system/) | M/S 系列控制器、CHARMs 电子配线、ISA-88 批处理控制、ACN 通信、1:1 冗余 | MIT 2.171, Purdue ME575, ISA-88 |
| [mini-fieldbus-foundation-h1](mini-fieldbus-foundation-h1/) | H1 物理层（IEC 61158-2）、LAS 调度、FBAP/FMS 应用层、设备互操作性、网段工程 | MIT 2.171, Stanford ENGR205, CMU 24-677 |
| [mini-honeywell-experion-pks](mini-honeywell-experion-pks/) | C300 控制器扫描/执行、CEE 调度、控制模块、CAB 大容量 I/O、HMIWeb 显示、趋势系统 | MIT 6.302, MIT 2.171, CMU 18-771, CMU 24-677 |
| [mini-supcon-ecs700-dcs](mini-supcon-ecs700-dcs/) | SCnet 冗余以太网、控制站架构、SBUS I/O 子系统、多层冗余、ECS-700 系统核心 | MIT 2.171, CMU 24-677 |
| [mini-yokogawa-centum-vp](mini-yokogawa-centum-vp/) | Vnet/IP 通信、FCS 现场控制站、PID 控制模块、配对冗余、ISA-88 批处理管理 | MIT 2.171, MIT 6.302, CMU 24-677, ISA-88 |

## 设计理念

- **零外部依赖** — 纯 C（C99/C11），仅使用 `libc` 和 `libm`
- **模块自包含** — 每个目录自带 `Makefile`、`include/`、`src/`、`examples/`、`demos/`、`tests/`
- **工业标准对齐** — 每个模块均映射到实际工业标准（ISA-18.2、ISA-88、ISA-95、IEC 61158、IEC 61508、IEC 61511）
- **真实平台建模** — 四个商用 DCS 平台（DeltaV、Experion PKS、ECS-700、CENTUM VP），包含实际硬件定义、扫描周期和冗余拓扑

## 构建方式

每个模块相互独立。进入模块目录后运行：

```bash
cd mini-dcs-alarm-management-isa182
make all    # 构建全部
make test   # 运行测试
```

需要 **GCC** 和 **GNU Make**。

## 项目结构

```
mini-dcs-architecture-application/
├── mini-dcs-alarm-management-isa182/   # ISA-18.2 报警生命周期与 KPI 指标
├── mini-dcs-redundancy-failover/       # 冗余、故障切换与可用性模型
├── mini-dcs-system-architecture/       # ISA-95 层级映射与 DCS 核心架构
├── mini-emerson-deltav-system/         # Emerson DeltaV M/S 系列控制器模型
├── mini-fieldbus-foundation-h1/        # Foundation Fieldbus H1 协议栈（IEC 61158）
├── mini-honeywell-experion-pks/        # Honeywell Experion PKS C300/CEE 平台
├── mini-supcon-ecs700-dcs/             # SUPCON ECS-700 SCnet / 控制站模型
└── mini-yokogawa-centum-vp/            # Yokogawa CENTUM VP Vnet/IP / FCS 模型
```

## 许可证

MIT
