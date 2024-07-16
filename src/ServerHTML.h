// library feita por Arthur Pinheiro Côrtes para uso em seu TCC.

// size of buffer used to capture HTTP requests
#define REQ_BUF_SZ   128

void sd_begin()
{
  // initialize SD card
  Serial.println("Initializing SD card...");
  if (!SD.begin(4)) {
    Serial.println("ERROR - SD card initialization failed!");
    return;    // init failed
  }
  Serial.println("SUCCESS - SD card initialized.");
  // check for index.htm file
  if (!SD.exists("index.htm")) {
    Serial.println("ERROR - Can't find index.htm file!");
    return;  // can't find index file
  }
  Serial.println(F("SUCCESS - Found index.htm file."));
}
