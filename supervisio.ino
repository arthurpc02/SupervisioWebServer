// colocar controle on-off

#include <DS1307.h>
#include <SPI.h>
#include <Ethernet.h>
#include <SD.h>
#include "ServerHTML.h"

#define PIN_bomba 5
byte STATE_bomba = 0;

#include <dht.h>
#define PIN_sensor 6
dht DHT;
float umidade = 42;
float temperatura = 42;
unsigned long timer_sensor = 0;
int timeout_sensor = 2000; // intervalo de leitura do sensor
#define PIN_sensor2 7
dht DHT2;
int umidade2 = 42;
int temperatura2 = 42;


// inicializa relógio RTC:
#include <DS1307.h>
DS1307  rtc(SDA, SCL);
Time  t;
unsigned long timer_rtc = 0;
int timeout_rtc = 500; // intervalo de leitura das horas
int relogio_hr = 0;
int relogio_min = 0;

//log function:
unsigned long timer_log = 0;
int timeout_log = 10000;

//control function
#define stg_disabled 0
#define stg_off 1
#define stg_on 2
int estagio_control = stg_disabled;
int umid_min = 10;
int tolerancia = 5;
byte control_status = HIGH;
int dur_control = 10; //máximo de minutos que a bomba pode ficar ligada para controlar umidade
int timer_control = 0;   //usado para controlar a duração da ação de controle
int countdown_control = dur_control;  //usado para controlar a duração da ação de controle
int timer_descanso = 99;   // descansa a ação de controle por 1 hr

// Ethernet shield e variáveis do servidor:
byte mac[] = { 0xDE, 0xFA, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 12); // IP address, may need to change depending on network
EthernetServer server(80);  // create a server at port 80
char HTTP_req[REQ_BUF_SZ] = {0}; // buffered HTTP request stored as null terminated string
char req_index = 0;              // index into HTTP_req buffer

// cartão SD:
File webFile;               // the web page file on the SD card

// Progs: ligações programadas da bomba
byte automatico = 1; // quando for igual a 0, opera em modo manual.

// variáveis do prog1:
byte prog1_status = 0;  //se estiver desmarcado(0), significa que essa prog deve ser ignorada
int prog1_hr = 21;
int prog1_min = 8;
int prog1_dur = 1;
int countdown1 = 0;
int timer1 = 0;

// variáveis do prog2:
byte prog2_status = 0;  //se estiver desmarcado(0), significa que essa prog deve ser ignorada
int prog2_hr = 21;
int prog2_min = 8;
int prog2_dur = 1;
int countdown2 = 0;
int timer2 = 0;

// variáveis do prog3:
byte prog3_status = 0;  //se estiver desmarcado(0), significa que essa prog deve ser ignorada
int prog3_hr = 21;
int prog3_min = 8;
int prog3_dur = 1;
int countdown3 = 0;
int timer3 = 0;

void setup()
{
  // disable Ethernet chip
  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH);
  pinMode(53, OUTPUT);

  pinMode(PIN_bomba, OUTPUT);

  Serial.begin(115200);       //for debugging

  sd_begin();

  rtc.begin();
  rtc.halt(false); // Set the clock to run-mode

  Ethernet.begin(mac, ip);  // initialize Ethernet device
  server.begin();           // start to listen for clients
}

/******************************************************
 ********************* VOID LOOP  *********************
 *****************************************************/

void loop()
{
  check_server();
  read_time(); //leitura do RTC
  read_sensor();
  data_log();

  if (automatico == 1)
  {
    check_progs();
    controle_umid();
  }
  else
  {
    prog1_status = 0;
    prog2_status = 0;
    prog3_status = 0;
    control_status = 0;
  }

  acionamento_bomba();

}


/***********************************************************
 ********************* OUTRAS FUNÇÕES *********************
 ***********************************************************/

void check_server()
{
  EthernetClient client = server.available();  // try to get client
  if (client) // got client?
  {
    boolean currentLineIsBlank = true;
    while (client.connected())
    {
      if (client.available()) // client data available to read
      {
        char c = client.read(); // read 1 byte (character) from client
        // limit the size of the stored received HTTP request
        // buffer first part of HTTP request in HTTP_req array (string)
        // leave last element in array as 0 to null terminate string (REQ_BUF_SZ - 1)
        if (req_index < (REQ_BUF_SZ - 1)) {
          HTTP_req[req_index] = c;          // save HTTP request character
          req_index++;
        }
        // last line of client request is blank and ends with \n
        // respond to client only after last line received
        if (c == '\n' && currentLineIsBlank) { // send a standard http response header


          // remainder of header follows below, depending on if
          // web page or XML page is requested
          // Ajax request - send XML file

          if (StrContains(HTTP_req, "uP"))  //uP = update page
          {
            XML_response(client);
          }

          else if (StrContains(HTTP_req, "gS")) //gS = get status (from progs)
          {
            prog1_status = parseStatus('1');
            prog2_status = parseStatus('2');
            prog3_status = parseStatus('3');
            control_status = parseStatus('C');
            Serial.print("received control_status = "); Serial.println(control_status);
          }

          else if (StrContains(HTTP_req, "aB")) //aB = acionar bomba (from progs)
          {
            STATE_bomba = parseStatus('B');
          }

          else if (StrContains(HTTP_req, "aT"))  //aT = automatico
          {
            Serial.println("Request contains 'aT' ");
            automatico = parseStatus('A');
            Serial.print("automatico: "); Serial.println(automatico);
          }

          else if (StrContains(HTTP_req, "sT"))  //sT = set time
          {
            Serial.println("sT: ajustar relogio para: ");
            relogio_hr = parseHoras();
            Serial.print(F("hora: ")); Serial.println(relogio_hr);
            relogio_min = parseMinutos();
            Serial.print(F("minutos: ")); Serial.println(relogio_min);
            set_time();
          }

          else if (StrContains(HTTP_req, "sP1")) //sP1 = set prog1
          {
            Serial.println("sP1: marcar para: ");
            prog1_hr = parseHoras();
            Serial.print(F("hora: ")); Serial.println(prog1_hr);
            prog1_min = parseMinutos();
            Serial.print(F("minutos: ")); Serial.println(prog1_min);
            prog1_dur = parseDuration();
            Serial.print(F("duracao: ")); Serial.println(prog1_dur);
          }

          else if (StrContains(HTTP_req, "sP2")) //sP2 = set prog2
          {
            Serial.println("sP2: marcar para: ");
            prog2_hr = parseHoras();
            Serial.print(F("hora: ")); Serial.println(prog2_hr);
            prog2_min = parseMinutos();
            Serial.print(F("minutos: ")); Serial.println(prog2_min);
            prog2_dur = parseDuration();
            Serial.print(F("duracao: ")); Serial.println(prog2_dur);
          }

          else if (StrContains(HTTP_req, "sP3")) //sP3 = set prog3
          {
            Serial.println("sP3: marcar para: ");
            prog3_hr = parseHoras();
            Serial.print(F("hora: ")); Serial.println(prog3_hr);
            prog3_min = parseMinutos();
            Serial.print(F("minutos: ")); Serial.println(prog3_min);
            prog3_dur = parseDuration();
            Serial.print(F("duracao: ")); Serial.println(prog3_dur);
          }

          else if (StrContains(HTTP_req, "hC")) //hC = humidity control
          {
            Serial.println(F("Controle de umidade: "));
            umid_min = parseValue('U');
            Serial.print(F("Umidade minima permitida: ")); Serial.println(umid_min);
            tolerancia = parseValue('T');
            Serial.print(F("tolerancia (histerese): ")); Serial.println(tolerancia);
          }

          else {  // web page request
            client.println("HTTP/1.1 200 OK");
            // send rest of HTTP header
            client.println("Content-Type: text/html");
            client.println("Connection: keep-alive");
            client.println();
            // send web page
            webFile = SD.open("index.htm");        // open web page file
            if (webFile) {
              while (webFile.available()) {
                client.write(webFile.read()); // send web page to client
              }
              webFile.close();
            }
          }
          // display received HTTP request on serial port
          //           Serial.print(HTTP_req);
          // reset buffer index and all buffer elements to 0
          req_index = 0;
          StrClear(HTTP_req, REQ_BUF_SZ);
          break;
        }
        // every line of text received from the client ends with \r\n
        if (c == '\n') {
          // last character on line of received text
          // starting new line with next character read
          currentLineIsBlank = true;
        }
        else if (c != '\r') {
          // a text character was received from client
          currentLineIsBlank = false;
        }
      } // end if (client.available())
    } // end while (client.connected())
    delay(1);      // give the web browser time to receive the data
    client.stop(); // close the connection
  } // end if (client)
}

void read_sensor()
{
  if (millis() - timer_sensor >= timeout_sensor)
  {
    int chk = DHT.read22(PIN_sensor);   // para DHT11
    //int chk2 = DHT2.read11(PIN_sensor2);  // para DHT22
    switch (chk)  // Trata erros de leitura do sensor
    {
      case DHTLIB_OK:
        //Serial.print("OK,\t");
        break;
      case DHTLIB_ERROR_CHECKSUM:
        Serial.println("Sensor read: Checksum error");
        break;
      case DHTLIB_ERROR_TIMEOUT:
        Serial.println("Sensor read: Time out error");
        break;
      default:
        Serial.println("Sensor read: Unknown error");
        break;
    }

    umidade = DHT.humidity;
    temperatura = DHT.temperature;
//    umidade2 = DHT2.humidity;
//    temperatura2 = DHT2.temperature;



    timer_sensor = millis();
//    Serial.print("status controle: "); Serial.print(control_status);
//    Serial.print("minutos: "); Serial.print(t.min); Serial.print(" countdown: "); Serial.print(countdown_control);
    Serial.print("  umidade: "); Serial.print(umidade); Serial.print("  temperatura: "); Serial.println(temperatura);
//    Serial.print("  umidade2: "); Serial.print(umidade2); Serial.print("  temperatura2: "); Serial.println(temperatura2);
  }
}

void read_time()
{
  if (millis() - timer_rtc >= timeout_rtc)
  {
    t = rtc.getTime();
    timer_rtc = millis();
    //Serial.print("time: ");Serial.print(t.hour);Serial.print(":");Serial.println(t.min);
  }

}

void data_log()
{
  if (millis() - timer_log > timeout_log)
  {
    File dataFile = SD.open("datalog.txt", FILE_WRITE);
    if (dataFile)
    {
      dataFile.print(t.year); dataFile.print("/");
      dataFile.print(t.mon); dataFile.print("/");
      dataFile.print(t.date); dataFile.print(",");
      dataFile.print(t.hour); dataFile.print(":");
      dataFile.print(t.min); dataFile.print(":");
      dataFile.print(t.sec); dataFile.print(",");
      dataFile.print(STATE_bomba); dataFile.print(",");
      dataFile.print(temperatura); dataFile.print(",");
      dataFile.print(umidade); 
//      dataFile.print(",");
//      dataFile.print(temperatura2); dataFile.print(",");
//      dataFile.print(umidade2);
      dataFile.println();
      dataFile.close();
    }
    else
    {
      Serial.println("error opening datalog.txt");
    }
    timer_log = millis();
  }
}

void controle_umid()
{
  switch (estagio_control)
  {
    case stg_disabled:
      if (control_status == HIGH)
      {
        estagio_control = stg_off;
        //Serial.println("stg_disabled to stg_off");
      }
      break;

    case stg_off:
      if (umidade < umid_min && countdown_control > 0)
      {
        estagio_control = stg_on;
//        Serial.println("stg_off to stg_on");
        timer_control = t.min;
        //Serial.print(timer_control); Serial.print("  "); Serial.println(t.min);
      }

      if (t.hour != timer_descanso)
      {
        countdown_control = dur_control;
      }

      if (control_status == LOW)
      {
        estagio_control = stg_disabled;
       // Serial.println("stg_off to stg_disabled");
        countdown_control = dur_control;
      }
      break;

    case stg_on:
      STATE_bomba = HIGH;

      if (timer_control != t.min)
      {
       // Serial.print(" -1  "); Serial.print(timer_control); Serial.print("  "); Serial.println(t.min);
        countdown_control--;
        timer_control = t.min;
      }

      if (umidade > (umid_min + tolerancia) || countdown_control <= 0)
      {
        estagio_control = stg_off;
       // Serial.println("stg_on to stg_off");
        timer_descanso = t.hour;
        STATE_bomba = LOW;
      }

      if (control_status == LOW)
      {
        estagio_control = stg_disabled;
      //  Serial.println("stg_on to stg_disabled");
        countdown_control = dur_control;
        STATE_bomba = LOW;
      }
      break;
  }
}


void check_progs()
{
  if (prog1_status == HIGH)
  {
    if (t.hour == prog1_hr)
    {
      if (t.min == prog1_min)
      {
        STATE_bomba = HIGH;
        countdown1 = prog1_dur;
        timer1 = t.min;
      }
    }

    if (STATE_bomba == HIGH)
    {
      if (t.min != timer1)
      {
        countdown1--;
        timer1 = t.min;
        if (countdown1 <= 0)
        {
          STATE_bomba = LOW;
        }
      }
    }
  }

  if (prog2_status == HIGH)
  {
    if (t.hour == prog2_hr)
    {
      if (t.min == prog2_min)
      {
        STATE_bomba = HIGH;
        countdown2 = prog2_dur;
        timer2 = t.min;
      }
    }

    if (STATE_bomba == HIGH)
    {
      if (t.min != timer2)
      {
        countdown2--;
        timer2 = t.min;
        if (countdown2 <= 0)
        {
          STATE_bomba = LOW;
        }
      }
    }
  }

  if (prog3_status == HIGH)
  {
    if (t.hour == prog3_hr)
    {
      if (t.min == prog3_min)
      {
        STATE_bomba = HIGH;
        countdown3 = prog3_dur;
        timer3 = t.min;
      }
    }

    if (STATE_bomba == HIGH)
    {
      if (t.min != timer3)
      {
        countdown3--;
        timer3 = t.min;
        if (countdown3 <= 0)
        {
          STATE_bomba = LOW;
        }
      }
    }
  }
}

void acionamento_bomba()
{
  if (STATE_bomba == HIGH)
  {
    digitalWrite(PIN_bomba, LOW);
   // tone(PIN_bomba, 262);
  }
  else
  {
    digitalWrite(PIN_bomba, HIGH);
   //  noTone(PIN_bomba);
  }
}

void set_time()
{
  rtc.setTime(char(relogio_hr), char(relogio_min), 0);     // Set the time to 12:00:00 (24hr format)
}

// Envia arquivo XML com relogio, prog1, prog2, prog3, status da bomba e leitura do sensor
void XML_response(EthernetClient cl)
{
  cl.println("HTTP/1.1 200 OK");
  cl.println("Content-Type: text/xml");
  cl.println("Connection: keep-alive");
  cl.println();
  cl.println("<?xml version = \"1.0\" ?>");
  cl.println("<inputs>");

  cl.print("<relogio>");
  cl.print(String(t.hour));
  cl.println("</relogio>");
  cl.print("<relogio>");
  cl.print(String(t.min));
  cl.println("</relogio>");

  cl.print("<prog1>");
  cl.print(String(prog1_status));
  cl.println("</prog1>");
  cl.print("<prog1>");
  cl.print(String(prog1_hr));
  cl.println("</prog1>");
  cl.print("<prog1>");
  cl.print(String(prog1_min));
  cl.println("</prog1>");
  cl.print("<prog1>");
  cl.print(String(prog1_dur));
  cl.println("</prog1>");

  cl.print("<prog2>");
  cl.print(String(prog2_status));
  cl.println("</prog2>");
  cl.print("<prog2>");
  cl.print(String(prog2_hr));
  cl.println("</prog2>");
  cl.print("<prog2>");
  cl.print(String(prog2_min));
  cl.println("</prog2>");
  cl.print("<prog2>");
  cl.print(String(prog2_dur));
  cl.println("</prog2>");

  cl.print("<prog3>");
  cl.print(String(prog3_status));
  cl.println("</prog3>");
  cl.print("<prog3>");
  cl.print(String(prog3_hr));
  cl.println("</prog3>");
  cl.print("<prog3>");
  cl.print(String(prog3_min));
  cl.println("</prog3>");
  cl.print("<prog3>");
  cl.print(String(prog3_dur));
  cl.println("</prog3>");

  cl.print("<control>");
  cl.print(String(control_status));
  cl.println("</control>");
  cl.print("<control>");
  cl.print(String(umid_min));
  cl.println("</control>");
  cl.print("<control>");
  cl.print(String(tolerancia));
  cl.println("</control>");

  cl.print("<bomba>");
  cl.print(String(STATE_bomba));
  cl.println("</bomba>");

  cl.print("<automatico>");
  cl.print(String(automatico));
  cl.println("</automatico>");

  cl.print("<sensor>");
  cl.print(String(umidade));
  cl.println("</sensor>");
  cl.print("<sensor>");
  cl.print(String(temperatura));
  cl.println("</sensor>");

  cl.print("</inputs>");
}





// FUNÇÕES PARA TRATAMENTO DE STRINGS:

// sets every element of str to 0 (clears array)
void StrClear(char *str, char length)
{
  for (int i = 0; i < length; i++) {
    str[i] = 0;
  }
}


// searches for the string sfind in the string str
// returns 1 if string found
// returns 0 if string not found
char StrContains(char *str, char *sfind)
{
  char found = 0;
  char index = 0;
  char len;

  len = strlen(str);

  if (strlen(sfind) > len) {
    return 0;
  }
  while (index < len) {
    if (str[index] == sfind[found]) {
      found++;
      if (strlen(sfind) == found) {
        return 1;
      }
    }
    else {
      found = 0;
    }
    index++;
  }

  return 0;
}

byte parseStatus(char num)
{
  byte chk = 0;
  for (int j = 0; j < REQ_BUF_SZ; j++)
  {
    if (HTTP_req[j] == '&')
    {
      if (HTTP_req[j + 1] == 's') //coleta horas
      {
        if (HTTP_req[j + 2] == 't')
        {
          //          Serial.println("&st");
          if (HTTP_req[j + 3] == num)
          {
            //            Serial.println("&st1");
            if (HTTP_req[j + 5] == '1')
            {
              //              Serial.println("chk = 1");
              chk = 1;

            }
          }
        }
      }
    }
  }
  return chk;
}

int parseHoras()
{
  char horas[3] = {0};
  int dado = 99;
  for (int j = 0; j < REQ_BUF_SZ; j++)
  {
    if (HTTP_req[j] == '&')
    {
      if (HTTP_req[j + 1] == 'h') //coleta horas
      {
        if (HTTP_req[j + 2] == 'r')
        {
          if (HTTP_req[j + 5] != '&')
          {
            horas[0] = HTTP_req[j + 4];
            horas[1] = HTTP_req[j + 5];
            dado = atoi(horas);
          }
          else
          {
            horas[0] = HTTP_req[j + 4];
            dado = atoi(horas);
          }
        }
      }
    }
  }
  return dado;   // se o valor retornado foi 99, é porque ocorreu um erro.
}

int parseMinutos()
{
  char minutos[3] = {0};
  int dado = 99;
  for (int j = 0; j < REQ_BUF_SZ; j++)
  {
    if (HTTP_req[j + 1] == 'm') // coleta minutos
    {
      if (HTTP_req[j + 2] == 'n')
      {
        if (HTTP_req[j + 5] != '&')
        {
          minutos[0] = HTTP_req[j + 4];
          minutos[1] = HTTP_req[j + 5];
          dado = atoi(minutos);
        }
        else
        {
          minutos[0] = HTTP_req[j + 4];
          dado = atoi(minutos);
        }
      }
    }
  }
  return dado;   // se o valor retornado foi 99, é porque ocorreu um erro.
}

int parseDuration()
{
  char duration[3] = {0};
  int dado = 99;
  for (int j = 0; j < REQ_BUF_SZ; j++)
  {
    if (HTTP_req[j + 1] == 'd') // coleta minutos
    {
      if (HTTP_req[j + 2] == 'r')
      {
        if (HTTP_req[j + 5] != '&')
        {
          duration[0] = HTTP_req[j + 4];
          duration[1] = HTTP_req[j + 5];
          dado = atoi(duration);
        }
        else
        {
          duration[0] = HTTP_req[j + 4];
          dado = atoi(duration);
        }
      }
    }
  }
  return dado;   // se o valor retornado foi 99, é porque ocorreu um erro.
}

int parseValue(char num)
{
  char value[3] = {0};
  int dado = 99;

  for (int j = 0; j < REQ_BUF_SZ; j++)
  {
    if (HTTP_req[j] == '&')
    {
      if (HTTP_req[j + 1] == 'v') //coleta horas
      {
        if (HTTP_req[j + 2] == 'l')
        {
          //          Serial.println("&st");
          if (HTTP_req[j + 3] == num)
          {
            if (HTTP_req[j + 6] != '&')
            {
              value[0] = HTTP_req[j + 5];
              value[1] = HTTP_req[j + 6];
              dado = atoi(value);
            }
            else
            {
              value[0] = HTTP_req[j + 5];
              dado = atoi(value);
            }
          }
        }
      }
    }
  }
  return dado;   // se o valor retornado foi 99, é porque ocorreu um erro.
}


