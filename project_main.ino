#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
MFRC522 mfrc522(10, 9);  // SS_PIN=10, RST_PIN=9

// Pin definitions
#define LED_Y 7
#define LED_R 6
#define BUZZER 8
#define v1 2
#define v2 3
#define v3 4
#define v4 5

// Voting and state variables
int vote1 = 0, vote2 = 0, vote3 = 0;
bool votedFlags[6] = { false };  // Flags for 6 voters

// Known UIDs
const String VOTER_UIDS[6] = {
  "92 2B FA 03", "D3 11 6A AD", "93 0D F9 03",
  "D2 B6 34 03", "E3 3F F9 03", "79 92 FA 03"
};
const String AUTHORITY_UID = "53 4C 9A 36";

void setup() {
  Serial.begin(9600);
  SPI.begin();
  mfrc522.PCD_Init();
  lcd.init();
  lcd.backlight();

  pinMode(LED_Y, OUTPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  noTone(BUZZER);

  // Enable internal pullups for buttons
  pinMode(v1, INPUT_PULLUP);
  pinMode(v2, INPUT_PULLUP);
  pinMode(v3, INPUT_PULLUP);
  pinMode(v4, INPUT_PULLUP);
}

void loop() {
  // Wait for new card
  if (!mfrc522.PICC_IsNewCardPresent()) {
    displayIdleScreen();
    return;
  }

  // Read card
  if (!mfrc522.PICC_ReadCardSerial()) return;

  // Get UID
  String content = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
    content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  content.toUpperCase();

  // Halt PICC to prevent repeated reads
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  lcd.clear();

  // Authority card handling
  if (content.substring(1) == AUTHORITY_UID) {
    handleAuthorityCard();
    delay(1000);
    return;
  }

  // Voter card handling
  for (int i = 0; i < 6; i++) {
    if (content.substring(1) == VOTER_UIDS[i]) {
      if (!votedFlags[i]) {
        handleVoter(i);
      } else {
        displayAlreadyVoted();
      }
      return;
    }
  }

  // Unauthorized card
  displayUnauthorized();
}

// Helper functions

void displayIdleScreen() {
  lcd.setCursor(3, 0);
  lcd.print("SHOW YOUR");
  lcd.setCursor(4, 1);
  lcd.print("ID CARD");
}

void handleVoter(int voterIndex) {
  lcd.setCursor(4, 0);
  lcd.print("Welcome");
  tone(BUZZER, 500, 500);
  delay(2000);
  lcd.clear();

  // Wait for vote with 10s timeout
  unsigned long startTime = millis();
  while (millis() - startTime < 10000) {
    lcd.setCursor(4, 0);
    lcd.print("Vote Now");
    lcd.setCursor(0, 1);
    lcd.print("1->A  2->B  3->C");
    if (digitalRead(v1) == LOW) {
      votedFlags[voterIndex] = true;
      recordVote(1, "A");
      return;
    }
    if (digitalRead(v2) == LOW) {
      votedFlags[voterIndex] = true;
      recordVote(2, "B");
      return;
    }
    if (digitalRead(v3) == LOW) {
      votedFlags[voterIndex] = true;
      recordVote(3, "C");
      return;
    }
  }
  lcd.clear();
  lcd.print("Vote Timeout");
  digitalWrite(LED_R, HIGH);
  tone(BUZZER, 300);
  delay(1000);
  noTone(BUZZER);
  digitalWrite(LED_R, LOW);
  delay(1000);
  lcd.clear();
}

void recordVote(int candidate, String label) {
  lcd.clear();
  digitalWrite(LED_Y, HIGH);
  if (candidate == 1) vote1++;
  if (candidate == 2) vote2++;
  if (candidate == 3) vote3++;

  lcd.print("Voted for");
  lcd.setCursor(0, 1);
  lcd.print("Candidate " + label);
  tone(BUZZER, 700, 200);
  delay(2000);
  lcd.clear();
  digitalWrite(LED_Y, LOW);

  // Debounce release
  while (digitalRead(v1) == LOW || digitalRead(v2) == LOW || digitalRead(v3) == LOW)
    ;
  delay(100); // Debounce delay
}

void displayAlreadyVoted() {
  lcd.print("You already");
  lcd.setCursor(0, 1);
  lcd.print("voted");
  digitalWrite(LED_R, HIGH);
  tone(BUZZER, 300, 1000);
  delay(2000);
  digitalWrite(LED_R, LOW);
  lcd.clear();
}

void handleAuthorityCard() {
  lcd.print("Authority");
  digitalWrite(LED_Y, HIGH);
  tone(BUZZER, 500, 300);
  delay(2000);
  lcd.clear();

  // Display votes
  lcd.setCursor(1, 0);
  lcd.print("A");
  lcd.setCursor(1, 1);
  lcd.print(vote1);
  lcd.setCursor(5, 0);
  lcd.print("B");
  lcd.setCursor(5, 1);
  lcd.print(vote2);
  lcd.setCursor(9, 0);
  lcd.print("C");
  lcd.setCursor(9, 1);
  lcd.print(vote3);

  // Wait for result/continue button
  while ((digitalRead(v4) == HIGH) && (digitalRead(v1) == HIGH));

  if (digitalRead(v4) == LOW) {
    determineWinner();
    resetSystem();
  } else if (digitalRead(v1) == LOW) {
    lcd.clear();
    lcd.print("Vote Continue");
    delay(2000);
    lcd.clear();
  }
  digitalWrite(LED_Y, LOW);
}

void determineWinner() {
  int totalVotes = vote1 + vote2 + vote3;
  lcd.clear();
  if (totalVotes == 0) {
    lcd.print("No Votes Polled");
  } else if (vote1 > vote2 && vote1 > vote3) {
    lcd.setCursor(5, 0);
    lcd.print("A Wins");
  } else if (vote2 > vote1 && vote2 > vote3) {
    lcd.setCursor(5, 0);
    lcd.print("B Wins");
  } else if (vote3 > vote1 && vote3 > vote2) {
    lcd.setCursor(5, 0);
    lcd.print("C Wins");
  } else if (vote1 == vote2 && vote1 > vote3) {
    lcd.print("A-B Tie");
  } else if (vote1 == vote3 && vote1 > vote2) {
    lcd.print("A-C Tie");
  } else if (vote2 == vote3 && vote2 > vote1) {
    lcd.print("B-C Tie");
  } else {
    lcd.print("Tie");
  }
  delay(2000);
  lcd.clear();
}

void resetSystem() {
  vote1 = vote2 = vote3 = 0;
  delay(1000);
  lcd.print("System Reset");
  delay(2000);
  lcd.clear();
  for (int i = 0; i < 6; i++) {
    votedFlags[i] = false;
  }
}

void displayUnauthorized() {
  lcd.print("UNAUTHORIZED");
  lcd.setCursor(0, 1);
  lcd.print("ACCESS");
  digitalWrite(LED_R, HIGH);
  tone(BUZZER, 300, 1000);
  delay(1000);
  digitalWrite(LED_R, LOW);
  lcd.clear();
}
