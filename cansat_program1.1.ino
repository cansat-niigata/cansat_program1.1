/*----------------------------------------*/
/* 走行プログラム　　　　　　             */
/*----------------------------------------*/
#include <SD.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <Servo.h>
#include <MsTimer2.h>
#include <UseMotor.h>
#include <PID.h>

TinyGPSPlus gps;

//SoftwareSerial----------------------------
#define rxPin A1 //データ受信用のピン
#define txPin A0 //データ送信用のピン
SoftwareSerial ss(rxPin, txPin);
//------------------------------------------

//SD----------------------------------------
File datafile; //csv
//File kml_log_file; //kml
#define SD_PIN 4 //SPI通信に使うピン
void SaveToSDcard();
//void PrintKML_header();
//void SaveToSDcardWithKML();
//void CloseKML();
//------------------------------------------

//Servo-------------------------------------
Servo myservo;
const int servoPin = 9; //9番ピン
void detouchParachute(int PinNum);
//------------------------------------------

//フライトピン------------------------------
const int FlightPin = 16;
//------------------------------------------

//gps(サンプルプログラムそのまま流用)------------------------------------------------
//static const double GOAL_LAT = 37.866434, GOAL_LON = 138.938515;//目的地の位置座標（コウガクブトウマエ）
//static const double GOAL_LAT = 37.867500, GOAL_LON = 138.937146;//目的地の位置座標（ブシツトウワキ）
static const double GOAL_LAT = 37.871406, GOAL_LON = 138.942138;//目的地の位置座標（ヤキュウジョウ）

double PRE_LAT = 0, PRE_LON = 0; //以前の位置情報
bool start_flag = false;
static void smartDelay(unsigned long ms);
double delta = 0;
const int x = 15; //delta*xが機体の旋回時間
static void printFloat(float val, bool valid, int len, int prec);
static void printInt(unsigned long val, bool valid, int len);
static void printDateTime(TinyGPSDate &d, TinyGPSTime &t);
static void printStr(const char *str, int len);
void PrintGPSInfo();
unsigned long distanceKmToGoal;
double courseToGoal;
//-----------------------------------------------------------------------------------

//モータ用の変数--------------------------------------------
const int rPWMPin = 5;
const int rPin = 18;
const int lPWMPin = 6;
const int lPin = 17;
UseMotor motor_r(rPWMPin, rPin);
UseMotor motor_l(lPWMPin, lPin);
double angularDifference(double goal, double myLocation);
//----------------------------------------------------------

//エンコーダ用変数----------------------------------------
volatile int r=0, l=0; //割り込み時に使用する関数の変数
void changePWM();
const int WARIKOMI_R = 1; //D3pin
const int WARIKOMI_L = 0; //D2pin
//---------------------------------------------------------


//PID制御関連--------------------------------------------------
PID pid_r, pid_l;
const int target1 = 4;
const int target2 = 2;
void changePWM();
void stackMove();
//--------------------------------------------------------------

//割り込み用関数--------------------------------------------------
void R(){
  r++;
}
void L(){
  l++;
}
//----------------------------------------------------------------

//その他--------------------------------------------------------
float meter = 1.0;
float close = 5.0;
bool onTheGround = false;
void PrintGPSInfo();
void warmingupGPS();
//---------------------------------------------------------------

void setup() {
  //ピンモードの設定------------
  pinMode(rPWMPin, OUTPUT);
  pinMode(rPin, OUTPUT);
  pinMode(lPWMPin, OUTPUT);
  pinMode(lPin, OUTPUT);
  pinMode(FlightPin, INPUT);

  //PID制御の設定---------------
  pid_r.set_Target(target1);
  pid_r.set_Kp(5);
  pid_r.set_conVar(170);
  pid_l.set_Target(target1);
  pid_l.set_Kp(5);
  pid_l.set_conVar(170);
  MsTimer2::set(500, changePWM);//タイマー割り込みの設定

  Serial.begin(9600);
  
  //-------------SDの設定-----------------
  pinMode(SD_PIN,OUTPUT); //SDに使うピンを設定
   
  if(!SD.begin(SD_PIN)){
    Serial.println("Card failed, or not present");
  }else{
    Serial.println("Card initialized.");
    datafile = SD.open("log.csv", FILE_WRITE);
    //kml_log_file = SD.open("log.kml", FILE_WRITE);
  }
  digitalWrite(SD_PIN,HIGH); //SDの回線を遮断
  //---------------------------------------
    
  ss.begin(9600);//A0, A1ピンでシリアル通信を使用

  //割り込みの設定
  attachInterrupt(WARIKOMI_R, R, CHANGE);
  attachInterrupt(WARIKOMI_L, L, CHANGE);  
}

void loop(){
  if(!onTheGround){
    int flightPin = analogRead(FlightPin);
    Serial.print("FlightPin:");
    Serial.println(flightPin);
    if(flightPin < 50){
      delay(30000);     //30000に設定する
      //パラシュートを切り離す
      detouchParachute(servoPin);
      datafile.print("detouchParachute");
      datafile.println(',');
      
      //まずはパラシュートから離れるために直進する(10s)
      datafile.print("first move");
      datafile.println(',');
      Serial.println("First Move");
      float t = millis();
      motor_r.rotationNormal_p(pid_r.get_conVar());
      motor_l.rotationNormal_p(pid_l.get_conVar());
      MsTimer2::start();
      while(millis() - t < 10000)  //10000に設定する
        ;
      MsTimer2::stop();
      motor_r.stop();
      motor_l.stop();
      Serial.println("Finish!");

      //GPSの値が安定するまで待機
      //datafile.print("warming up");
      //datafile.println(',');
      //Serial.println("Warming Up start");
      //warmingupGPS();
      //Serial.println("Warming up done");
      
      datafile.print("All preparetion is Success");
      datafile.println(',');
      datafile.flush();

      //PrintKML_header();
      while(ss.available())
        ss.read();
      
      Serial.println(F("Sats  Latitude  Longitude   Date       Time     Course Speed Card  Distance Course Card  Chars Sentences Checksum  delta"));
      Serial.println(F("      (deg)     (deg)                           --- from GPS ----  ---- to London  ----  RX    RX        Fail"));
      Serial.println(F("---------------------------------------------------------------------------------------------------------------------------------"));
      onTheGround = true;
    }
    delay(100);  //あくまでデバックをみやすくするための記述
  }else {
    //GPSの値を取得する
    //値が更新されるまで待機
    while (ss.available() > 0){
      gps.encode(ss.read());
    }
    
    //GPSの情報をSerial通信でパソコンに送信
    if(gps.location.isUpdated()){
      //------------------GPS情報の表示--------------------
      PrintGPSInfo();
     distanceKmToGoal =(unsigned long)TinyGPSPlus::distanceBetween(
          gps.location.lat(),
          gps.location.lng(),
          GOAL_LAT, 
          GOAL_LON);
      Serial.print(distanceKmToGoal);
      Serial.print(",  ");

      courseToGoal =TinyGPSPlus::courseTo(
          gps.location.lat(),
          gps.location.lng(),
          GOAL_LAT, 
          GOAL_LON);
      Serial.print(courseToGoal);
      Serial.print(",  ");
      const char *cardinalToGoal = TinyGPSPlus::cardinal(courseToGoal);
      Serial.print(cardinalToGoal);
      Serial.print(",  ");
      Serial.print(gps.charsProcessed());
      Serial.print(",  ");
      Serial.print(gps.sentencesWithFix());
      Serial.print(",  ");
      Serial.print(gps.failedChecksum());
      Serial.print(",  ");
  
      double direction = TinyGPSPlus::courseTo(
        PRE_LAT,
        PRE_LON,
        gps.location.lat(),
        gps.location.lng());
      //-------------------------------------------------------------
      //-----------------制御に使用する位置情報の更新----------------
      PRE_LAT = gps.location.lat();
      PRE_LON = gps.location.lng();
      //同時に目的地までの距離、進行方向と目的地の方向の角度差θを計算
      //θは-180～180の間になるように調整
      delta = angularDifference(courseToGoal, direction);  //ゴール方向と移動方向のなす角
      Serial.println(delta);
      SaveToSDcard();
      //SaveToSDcardWithKML();
      MsTimer2::stop();
      //目的地までの距離がゴール判定の基準に達したか？
      //目的地までの距離が十分近ければゴールしたと判断する
      if(distanceKmToGoal < meter){
        //制御終了
        motor_r.stop();
        motor_l.stop();
        Serial.println("GOAL!!");
        datafile.println();
        datafile.println("GOAL");
        datafile.close();
        //CloseKML();
        while(true)
          ;
      }else {
        /*if(PRE_LAT == gps.location.lat() && PRE_LON == gps.location.lng()){
          stackMove();
          Serial.println("STACK");
        }*/
        if(distanceKmToGoal < close){
          pid_r.set_Target(target2);
          pid_l.set_Target(target2);
        }
        if(delta > 0){         //目的地から離れていた場合
          //deltaの値が正の時
          //左タイヤをdeltaに比例した量だけ回転
          motor_r.stop();
          motor_l.stop();
          motor_l.rotationNormal_p(pid_l.get_conVar());
          delay(x*delta);
        }else{
          //θの値が負の時
          //右タイヤをdeltaに比例した量だけ回転
          delta *= -1;
          motor_r.stop();
          motor_l.stop();
          motor_r.rotationNormal_p(pid_r.get_conVar());
          delay(x*delta);
        }
        //直進(次のGPSの値取得まで行う)
        //この時PID制御を行う
        //タイマー割り込みを利用
        r = 0; l = 0;
        motor_r.rotationNormal_p(pid_r.get_conVar());
        motor_l.rotationNormal_p(pid_l.get_conVar());
        MsTimer2::start();
      }
    }
  }
}
//asdfjklasdf
