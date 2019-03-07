/*---------------------------------------------
 * GPSの情報をパソコンに表示するための関数
 --------------------------------------------*/
void PrintGPSInfo(){
  Serial.print(gps.satellites.value());
  Serial.print(",     ");
  Serial.print(gps.location.lat(),6);
  Serial.print(",  ");
  Serial.print(gps.location.lng(),6);
  Serial.print(",  ");
  char sz[32];
  sprintf(sz, "%02d/%02d/%02d ", gps.date.year(), gps.date.month(), gps.date.day());
  Serial.print(sz);
  Serial.print(",");
  sprintf(sz, "%02d:%02d:%02d ", gps.time.hour()+9, gps.time.minute(), gps.time.second());
  Serial.print(sz);
  Serial.print(",");
  Serial.print(gps.course.deg(),3);
  Serial.print(", ");
  Serial.print(gps.speed.kmph(),1);
  Serial.print(", ");
  Serial.print(TinyGPSPlus::cardinal(gps.course.deg()));
}
