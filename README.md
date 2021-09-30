# am_joyin

Amos Joystick Input Driver for Raspbrri-pi Arcade (or another SBC)

이것은 라즈베리파이를 이용하여 아케이드 게임기를 제작할 때 다양한 입력장치를 사용할 수 있도록 기획되고 제작되었다.

기본적으로 GPIO를 통해 조이스틱을 입력 받거나, 74HC165, MUX, MCP23017 등의 IO 확장 보드 등을 지원한다.

게임기를 제작하는 과정에서 다양한 형태의 입력 장치를 사용할 수 있고, 또한 이런 장치들의 조합 역시 다양할 수 있기에, 무엇보다 확장성과 유연성이 중점을 두고 설계되었다.

나는 이 드라이버 개발 과정에서 mk_arcade_joystick_rpi를 참고했다.
주요한 로직의 일부를 차용했으며, 기존에 mk_arcade_joystick_rpi의 사용자들의 편의를 위해 의도적으로 GPIO 핀맵의 호환성을 유지하려 노력했다.

> ***NOTE***: 
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


```shell
keyset1_cfg={0x01,-1,1},{0x00,-1,1},{0x13B,0,1},{0x13A,0,1},{0x130,0,1},{0x103,0,1},{0x102,0,1},{0x103,0,1},{0x102,0,1},{0x103,0,1}
```

**실행 방법**

```shell
sudo modprobe am_joyin
```




## 드라이버 설치

### 0. 사전 작업

먼저 사전 작업으로 2가지를 해야 합니다.

> 1. wifi 연결 및 ssh 활성화
> 2. 만약 mk_arcade_joystick_rpi 드라이버가 설치되어 있다면 제거

레트로파이 설정 메뉴에서 retropie-setup으로 들어가, 드라이버 항목에서 mk_arcade_joystick_rpi 를 제거합니다.

### 1. am_joyin 드라이버 설치

이제 본격적으로 설치 작업을 진행합니다.

1. 라즈베리파이에 ssh로 접속합니다.

2. 다음의 커맨드를 차례로 입력합니다.

```shell
> git clone https://github.com/amos42/am_joyin.git
> cd am_joyin
> ./utils/makepackage.sh 0.1.0 
> sudo dpkg -i build/am_joyin-0.1.0.deb
```
​
이렇게 하면 일단 드라이버는 설치 됩니다.

### 3. am_joyin 설정

다음으로는 드라이버 설정을 진행해야 합니다.

am_joyin 설정 파일을 엽니다.

```shell
> sudo nano /etc/modprobe.d/am_joyin.conf
```

mk_archde_joystick_rpi와 동일한 구성의 GPIO를 사용하고 싶다면 다음 항목을 입력하고 ctrl-x를 눌러 저장하고 빠집니다.

```
options am_joyin device1="gpio;;0,default"
```

### 4. 드라이버 부팅시 자동 로딩

이제 이 드라이버를 쓸 수 있게 활성화를 합니다.

드라이버 모듈 설정 파일을 엽니다.

```shell
> sudo nano /etc/modules-load.d/modules.conf
```

마지막 라인에 다음 항목을 추가하고 ctrl-x를 눌러 저장하고 빠집니다.

```
  .
  .
  .
am_joyin
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
> 그 이의 기종에 대한 동작은 보증하지 못 한다.
