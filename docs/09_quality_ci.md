# 품질 게이트와 CI

## 로컬 품질 게이트

```sh
sh scripts/run_all_tests.sh
```

실행 순서:

1. 깨끗한 빌드
2. 일반 회귀 테스트
3. ASan/UBSan 회귀 테스트
4. Fault Injection 및 정상 복귀
5. GCC `-fanalyzer` 정적 분석
6. 정상 SSD 자동 테스트와 증적 파일 생성

어느 단계든 실패하면 스크립트가 즉시 비정상 종료한다.

## 컴파일 품질

모든 C 대상은 다음 경고를 사용한다.

```text
-std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow
```

정적 분석 타깃은 `-fanalyzer -Werror`를 추가해 발견된 분석 경고를 빌드
실패로 처리한다.

## 동적 분석

`make test-sanitize`는 다음 대상을 ASan/UBSan으로 빌드하고 실행한다.

- 저장 단위 테스트
- SSD CLI 통합 테스트
- Test Shell 통합 테스트
- 자동 테스트 프레임워크 테스트
- 벤치마크 통합 테스트

## GitHub Actions

`.github/workflows/ci.yml`은 `master`/`main` push와 Pull Request에서 전체
품질 게이트를 실행한다. 성공 여부와 무관하게 생성된 로그·CSV·Markdown을
`validation-evidence` artifact로 업로드하도록 구성했다.

Fault Job의 성공은 결함 SSD가 정상 동작했다는 뜻이 아니다. 지정된 mutation이
예상 테스트에서 FAIL로 검출되고 정상 SSD 회귀가 PASS해야 Job이 성공한다.

## 확인된 원격 실행

2026-10-12 커밋 `8b0db5b`에서 첫 GitHub Actions 실행을 확인했다.

- [SSD Validation CI #1](https://github.com/psy1218/SSD-Test-shell-PJT/actions/runs/38228132749)
- Job `build-test-sanitize-fault`: SUCCESS
- `Full quality gate`: SUCCESS
- `Upload validation evidence`: SUCCESS
- Artifact `validation-evidence`: 생성 확인
