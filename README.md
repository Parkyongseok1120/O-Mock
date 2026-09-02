# O-Mock

> **Local LLM Agent MCP Validation Project / Unreal Engine 5 Multiplayer Prototype**

O-Mock은 제가 개발한 **UE5 Local LLM Agent MCP**를 실제 Unreal Engine 게임 개발 환경에서 검증하기 위해 제작한 실험용 멀티플레이 게임 프로토타입입니다.

단순한 코드 생성 테스트가 아니라,
로컬 LLM이 프로젝트의 코드를 탐색하고 수정하며 빌드·검증을 반복하는 Agent Workflow를
실제 게임 개발 과정에 적용할 수 있는지 확인하는 것을 목표로 제작했습니다.

## Project Overview

기본 오목 규칙에 아이템, 턴 조작, 미니게임을 결합한
최대 4인 전략 파티 오목 게임입니다.

### Implemented Features

- 오목 보드 생성 및 돌 배치
- 턴 진행 및 5목 승리 판정
- 2~4인 플레이어 지원
- 플레이 인원에 따른 보드 크기 변경
- 개인 제한 시간 및 턴 시스템
- 봉인 / 견인 / 강탈 / 턴 스킵 / 수호막 등의 아이템 시스템
- Listen Server 기반 멀티플레이
- Server Authority 기반 게임 판정
- Custom Room 생성 및 게임 규칙 설정
- 주기적인 Mini Game
- 기본 UI 및 게임 진행 상태 표시
- Bot / Balance Test를 위한 기초 통계 수집

## Development with Local LLM Agent MCP

O-Mock은 일반적인 AI 코드 보조 프로젝트가 아니라,
Qwen 3.8 27B + LM Studio + 직접 개발한 UE5 Local LLM Agent MCP를
실제 게임 제작 과정에 적용하기 위한 E2E 실험 프로젝트입니다.

MCP를 통해 모델이 Unreal Engine 프로젝트의

`Search → Read → Modify → Build → Validate → Retry`

과정을 직접 수행하도록 하였으며,
이를 통해 로컬 LLM 기반 개발 에이전트의 실사용 가능성과 한계를 검증했습니다.

## Experiment Result

게임의 핵심 프로토타입은 실제 플레이 가능한 상태까지 구현되었습니다.

다만 실험 과정에서 장시간 작업, 복잡한 네트워크/백엔드 디버깅,
긴 Context 유지 등의 영역에서는 여전히 모델 자체의 추론 능력이 병목으로 작용함을 확인했습니다.

이 프로젝트는 완성된 상용 게임을 목표로 하기보다,
**Local LLM Agent를 실제 언리얼 프로젝트에 적용하고 검증하기 위한 실험적 프로토타입으로 제작되었습니다.
