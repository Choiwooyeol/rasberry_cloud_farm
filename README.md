# rasberry_cloud_farm
### 2013041023 소프트웨어 최우열
### 환경: mysql + apache + php7.0 (데스크탑서버)
---

### 기능
<li>pt[0]쓰레드에서 getSensor함수를 통해 1ms마다 온도센서와 조도센서를 통해 온도값과 조도값을 읽어옵니다.
<li>pt[1]쓰레드에서 10초마다 light값과 temp값을 DB로 보냅니다. 
<li>pt[2]쓰레드에서 getSensor에서 신호를 보낼시 온도에따라 FAN을 작동합니다. 25도 이상일시 5초동안 켜졌다 꺼집니다.
<li>pt[3]쓰레드에서 getSensor에서 신호를 보낼시 조도가 일정값 이하면 led를 켭니다.
위와 같은 결과는 웹상에서 DB값을 읽어와 확인이 가능합니다.

![screen](https://user-images.githubusercontent.com/22064576/40380427-024ad804-5e34-11e8-8697-e0e6c0ac47a4.png)
