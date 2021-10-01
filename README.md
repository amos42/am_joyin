# am_joyin

Amos Joystick Input Driver for Raspbrri-pi Arcade (or another SBC)

이것은 라즈베리파이를 이용하여 아케이드 게임기를 제작할 때 다양한 입력장치를 사용할 수 있도록 기획되고 제작되었다.

기본적으로 GPIO를 통해 조이스틱을 입력 받거나, 74HC165, MUX, MCP23017 등의 IO 확장 보드 등을 지원한다.

게임기를 제작하는 과정에서 다양한 형태의 입력 장치를 사용할 수 있고, 또한 이런 장치들의 조합 역시 다양할 수 있기에, 무엇보다 확장성과 유연성이 중점을 두고 설계되었다.

나는 이 드라이버 개발 과정에서 mk_arcade_joystick_rpi를 참고했다. 주요한 로직의 일부를 차용했으며, 기존에 mk_arcade_joystick_rpi의 사용자들의 편의를 위해 의도적으로 GPIO 핀맵의 호환성을 유지하려 노력했다.

> ***NOTE:***\
> mk_arcade_joystick_rpi 프로젝트 사이트 : https://github.com/recalbox/mk_arcade_joystick_rpi


**키워드 설명**

| 키워드    |   설명                                                         | 최대 갯수          |
|----------|----------------------------------------------------------------|-------------------|
| keyset   | 라즈베리파이에서 인식하는 입력 정보의 집합                        | 개본 1개 + 최대 2개 |
| endpoint | 라즈베리파이에서 액세스 할 단위. /dev/input/js#의 형태로 생성된다. | 최대 4개           |
| device   | 입출력을 실제로 처리할 장치                                      | 최대 4개           |


**키셋**

커스텀 키에서 사용하는 코드는 input-event-condes.h 파일을 참고하면 된다.

[input-event-condes.h](extra/input-event-condes.h)

기본 제공 키셋 (keyset id = 0)

| 키코드       | 코드  | 최소값 | 최대값 |
|-------------|-------|--------|-------|
| ABS_Y       | 0x01  | -1     | 1     |
| ABS_X       | 0x00  | -1     | 1     |
| BTN_START   | 0x13B | 0      | 1     |
| BTN_SELECT  | 0x13A | 0      | 1     |
| BTN_A       | 0x130 | 0      | 1     |
| BTN_B       | 0x131 | 0      | 1     |
| BTN_X       | 0x133 | 0      | 1     |
| BTN_Y       | 0x134 | 0      | 1     |
| BTN_TL      | 0x136 | 0      | 1     |
| BTN_TR      | 0x137 | 0      | 1     |
| BTN_MODE    | 0x13C | 0      | 1     |
| BTN_TL2     | 0x138 | 0      | 1     |
| BTN_TR2     | 0x139 | 0      | 1     |
| BTN_TRIGGER | 0x120 | 0      | 1     |


## 드라이버 설치

### 0. 사전 작업 및 개발 환경 구축

먼저 사전 작업으로 2가지를 해야 한다.

> 1. wifi 연결 및 ssh 활성화
> 2. 라즈베리파이에 ssh로 접속합니다.
> 3. 만약 mk_arcade_joystick_rpi 드라이버가 설치되어 있다면 제거 :
>    * retropie 설정 메뉴에서 retropie-setup으로 들어가, 드라이버 항목에서 mk_arcade_joystick_rpi 를 제거한다.
>    * shell에서 sudo ~/RetroPie-Setup/retropie_setup.sh 를 실행하여, 드라이버 항목에서 mk_arcade_joystick_rpi 를 제거한다.


다음으로는 드라이버 빌드를 위한 환경을 구축한다.

**1. 패키지 업데이트**

```shell
sudo apt update
```

**2. 개발툴 설치**

```shell
sudo apt install -y --force-yes dkms cpp-4.7 gcc-4.7 git joystick
```

**3. 커널 헤더 설치**

```shell
sudo apt install -y --force-yes raspberrypi-kernel-headers
```


### 1. am_joyin 드라이버 설치

사전 작업이 마무리 되었다면 본격적으로 설치 작업을 진행한다.

**1. 드라이버 소스를 받는다.**

```shell
git clone https://github.com/amos42/am_joyin.git
```

**.deb 패키지를 생성 후 설치한다.**

```shell
cd am_joyin
./utils/makepackage.sh 0.1.0 
sudo dpkg -i build/am_joyin-0.1.0.deb
```
​
이 과정까지 거치면 드라이버 설치가 1차적으로 완료된다.


### 3. am_joyin 설정

다음으로는 드라이버 설정을 진행한다.

> ***NOTE:***\
> am_joyin 설정 파일 위치 : /etc/modprobe.d/am_joyin.conf

텍스트 에디터로 설정 파일을 연다.

```shell
sudo nano /etc/modprobe.d/am_joyin.conf
```

GPIO를 이용한 1P 입력 장치를 적용하고 싶다면 다음과 같이 입력하고 ctrl-x를 눌러 저장하고 종료한다.

```
options am_joyin device1="gpio;;0,default1"
```

> ***NOTE:***\
> 만약 이 과정을 생략하고 am_joyin 설정을 누락시키면 am_joyin은 기본 default 파라미터로 동작한다.\
> 이는 mk_arcade_joystick_rpio map=1과 같은 동작을 재현한다.


### 4. 드라이버 부팅시 자동 로딩

다음은 전원을 켤 때마다 자동으로 am_joyin 드라이버가 로딩되도록 하기 위한 과정이다.

드라이버 모듈 설정 파일을 연다.

```shell
sudo nano /etc/modules-load.d/modules.conf
```

마지막 라인에 다음 항목을 추가하고 ctrl-x를 눌러 저장하고 종료한다.

```
  .
  .
  .

am_joyin
```


## 설정 파라미터

### 기본 형식

am_joyin은 설정을 통해 다양한 조합의 장치들을 이용할 수 있다.

기본적으로 전체가 1줄짜리 설정으로 이루어져 있으며, 공백은 허용되지 않는다.

설정은 1개 이상의 파라미터(parameter)들로 구성되어 있으며, 각 파라미터들은 특수문자를 포함하기 때문에 "(quotation mark)로 감싼 문자열로 기술한다.

각 파라미터는 1개 이상의 섹션(section)들로 구성되어 있으며, 섹션들은 ;(semicolon)으로 구분한다.

즉, 기본 형태는 다음과 같다.

```shell
parameter1="section1;section2;..." parameter2="section1;section2;..."
```

section은 1개 이상의 값으로 구성되어 있으며, 각 값들은 ,(comma) 문자로 구분된다. 빈 값도 허용된다.

값은 다음 중 하나가 될 수 있다.

| 값     |  설명      | 형태              |
|--------|-----------|-------------------|
|        | null      |                   |
| 숫자   | 숫자값     | 0                 |
| 문자열 | 문자열 값   | hello            |
| 블럭   | 값들의 집합 | {1,hello,world}  |

즉, 최종적으로 다음과 같은 형태로 기술되게 된다.

```shell
param1="text1;default,10;test,1,{1,a},{2,b}" param2="text1;;test,,{2,b,0},{3,,0}"
```

이를 해석하면 다음과 같다.

```yaml
param1:
  section1:
    - text1
  section2:
    - default
    - 10
  section3:
    - test
    - 1
    - block1:
        - 1
        - a
    - block2:
        - 2
        - b
param2:
  section1:
    - text1
  section2: <null>
  section3:
    - test
    - <null>
    - block1:
      - 2
      - b
      - 0
    - block2:
      - 3
      - <null>
      - 0
```

### am_joyin의 파라미터들

am_joyin의 파라미터는 다음과 같다.

| 파라미터           |  설명         | 
|-------------------|---------------|
| keyset1 ~ keyset2 | keyset 설정   |
| endpoints         | endpoint 설정 |
| device1 ~ device4 | device 설정   |

**keyset 설정**

```shell
keyset1_cfg="{0x01,-1,1},{0x00,-1,1},{0x13B,0,1},{0x13A,0,1},{0x130,0,1},{0x103,0,1},{0x102,0,1},{0x103,0,1},{0x102,0,1},{0x103,0,1}"
```

**endpoint 설정**

```shell
endpoints="default,13;1,11"
```

**device 설정**
```shell
device1="gpio;;0,default1,12"
```

## 각 장치별 파라미터 포맷

### GPIO 입력

![GPIO Interface](images/mk_joystick_arcade_GPIOs.png)

- 1인용 기본 키 설정

```shell
sudo modprobe am_joyin device1="gpio;;0,default1,default"
```

- 2인용 설정

```shell
sudo modprobe am_joyin endpoints="default,12;default,12" device1="gpio;;0,default1,12;1,default2,12"
```

- 커스텀 키 설정

```shell
sudo modprobe am_joyin device1="gpio;;0,custom,1,{4,0x1,-1},{17,0x1,1},{27,0x0,-1},{22,0x0,1},{10,0x13b,1},{9,0x13a,1}"
```


### 74HC165 입력

```shell
sudo modprobe am_joyin device1="74hc165;16,20,21,,1;0,defaultdefault"
```

### MCP23017 입력

```shell
sudo modprobe am_joyin device1="mcp23017;0x20;0,defaultdefault"
```


### Multiplexer(=MUX) 입력

```shell
sudo modprobe am_joyin device1="mux;5,{26,19,13,6},default,1;0,default"
```


> ***NOTE***
> 이것은 라즈베리파이3B, 라즈베리파이3B+, 라즈베리파이4B에서 테스트 되었다.
> 그 이외의 기종에 대한 동작은 보증하지 못 한다.
