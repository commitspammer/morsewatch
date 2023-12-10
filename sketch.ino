#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define ABS(x) ((x) < 0 ? -(x) : (x))
#define DEVICE_ID "Room02"
#define SSID "despacito"
#define PASSWORD "despacito"
#define SERVER_URL "http://192.168.43.123:8080"
#define BUZZER_PIN 33
#define LDR_PIN 34
#define BUTTON_PIN 32
#define LED_PIN 23
#define BUFFER_SIZE 300
#define DIT '.'
#define DAT '_'
#define LETTER_SEP ' '
#define WORD_SEP '/'
#define BUTTON_MODE 0
#define LDR_MODE 1

typedef struct llbuffer {
	long long at[BUFFER_SIZE];
	int len;
} LLBuffer;

typedef struct cbuffer {
	char at[BUFFER_SIZE];
	int len;
} CBuffer;

char transition_table[255][2];
int dit_len = 0;
int listen_start = 0;
int input_mode = BUTTON_MODE;
int input_state = LOW;
int emitted = 1;
int danger = 0;
int danger_start = 0;
LLBuffer intervals = { {0}, 0 }; //these buffers have no overflow control
CBuffer morse = { {'\0'}, 0 };
CBuffer text = { {'\0'}, 0 };

void setup() {
	init_transition_table(transition_table);
	Serial.begin(115200);
	pinMode(LED_PIN, OUTPUT);
	pinMode(BUZZER_PIN, OUTPUT);
	pinMode(BUTTON_PIN, INPUT_PULLUP);
	digitalWrite(LED_PIN, HIGH);

	WiFi.begin(SSID, PASSWORD);
	Serial.print("Connecting");
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}
	Serial.println(WiFi.localIP());

	digitalWrite(LED_PIN, LOW);
	Serial.println("Ready");
}

void loop() {
	//delay(5);
	if (danger == 0)
		danger = check_danger(String(text.at));
	if (danger == 1) {
		Serial.println("DANGER");
		tone(BUZZER_PIN, 450);
		danger = 2;
		danger_start = millis();
	}
	if (danger == 2 && millis() - danger_start > 15000) {
		noTone(BUZZER_PIN);
		danger = 0;
	}
	int input = get_new_input_state(input_mode);
	if (emitted == 0 && millis() - listen_start >= 5000 && input != HIGH) {
		text.at[text.len] = '\0';
		Serial.println(emit_event(DEVICE_ID, String(text.at)));
		emitted = 1;
		intervals.len = 0;
		morse.len = 0;
		text.len = 0;
		text.at[text.len] = '\0';
	}
	if (input == input_state) {
		return;
	}
	intervals.at[intervals.len++] = millis() - listen_start;
	listen_start = millis();
	input_state = input;
	if (input == HIGH) {
		if (danger == 0) tone(BUZZER_PIN, 900);
		digitalWrite(LED_PIN, HIGH);
	} else {
		if (danger == 0) noTone(BUZZER_PIN);
		digitalWrite(LED_PIN, LOW);
	}
	print_intervals();
	to_morse(&intervals, &morse);
	print_morse();
	to_text(&morse, &text);
	print_text();
	emitted = 0;
}

int get_new_input_state(int mode) {
	switch (mode) {
	case BUTTON_MODE:
		return digitalRead(BUTTON_PIN) == LOW ? HIGH : LOW; //cuz it's pullup
	case LDR_MODE:
		return analogRead(LDR_PIN) < 900 ? HIGH : LOW;
	default:
		Serial.println("error: no input mode");
		return LOW;
	}
}

//assumes that the intervals alternate and the 2nd interval is a click
void to_morse(LLBuffer* intervals, CBuffer* morse) {
	morse->len = 0;
	if (intervals->len < 2) {
		return;
	}
	if (dit_len == 0) {
		dit_len = intervals->at[1];
	}
	for (int i = 1; i < intervals->len; i++) {
		long long interval = intervals->at[i];
		if (i % 2 == 0) {
			if (interval > dit_len*7) {
				morse->at[morse->len++] = WORD_SEP;
			} else if (interval > dit_len*3) {
				morse->at[morse->len++] = LETTER_SEP;
			} else {
				//nothing
			}
		} else {
			int dist_to_dit = ABS(dit_len - interval);
			int dist_to_dat = ABS(dit_len*3 - interval);
			int dist_to_third = ABS(dit_len/3 - interval);
			if (dist_to_dit < dist_to_dat && dist_to_dit < dist_to_third) {
				morse->at[morse->len++] = DIT;
			} else if (dist_to_dat < dist_to_dit && dist_to_dat < dist_to_third) {
				morse->at[morse->len++] = DAT;
			} else { //frequency correction (maybe we should also just clear the buffer every-so-often)
				//dit_len = intervals[i]; //TODO try a "dit_len cadidate algo", better than updating right away
				//morse_len = 0;
				//i = -1;
				morse->at[morse->len++] = DIT;
			}
		}
	}
}

void init_transition_table(char t[][2]) {
	t['?'][0]='?'; t['?'][1]='?';
	t['\0'][0]='E';t['\0'][1]='T';
	t['E'][0]='I'; t['E'][1]='A';
	t['I'][0]='S'; t['I'][1]='U';
	t['S'][0]='H'; t['S'][1]='V';
	t['H'][0]='?'; t['H'][1]='?';
	t['V'][0]='?'; t['V'][1]=1;
	t['U'][0]='F'; t['U'][1]='?';
	t['F'][0]='?'; t['F'][1]='?';
	t['A'][0]='R'; t['A'][1]='W';
	t['R'][0]='L'; t['R'][1]='?';
	t['W'][0]='P'; t['W'][1]='J';
	t['P'][0]='?'; t['P'][1]='?';
	t['J'][0]='?'; t['J'][1]='?';
	t['T'][0]='N'; t['T'][1]='M';
	t['N'][0]='D'; t['N'][1]='K';
	t['D'][0]='B'; t['D'][1]='X';
	t['B'][0]='?'; t['B'][1]='?';
	t['X'][0]='?'; t['X'][1]='?';
	t['K'][0]='C'; t['K'][1]='Y';
	t['C'][0]='?'; t['C'][1]='?';
	t['Y'][0]='?'; t['Y'][1]='?';
	t['M'][0]='G'; t['M'][1]='O';
	t['G'][0]='Z'; t['G'][1]='Q';
	t['Z'][0]='?'; t['Z'][1]='?';
	t['Q'][0]='?'; t['Q'][1]='?';
	t['O'][0]='?'; t['O'][1]='?';
	t[1][0]='?'; t[1][1]=2;
	t[2][0]=3; t[2][1]='?';
	t[3][0]=4; t[3][1]='?';
	t[4][0]=5; t[4][1]='?';
	t[5][0]=5; t[5][1]=5; //SOS
}

void write_SOS(CBuffer* buf) {
	buf->at[buf->len++] = 'S';
	buf->at[buf->len++] = 'O';
	buf->at[buf->len++] = 'S';
}

//assumes that "text" is allocated
void to_text(CBuffer* morse, CBuffer* text) {
	text->len = 0;
	char state = '\0';
	for (int i = 0; i < morse->len; i++) {
		if (morse->at[i] == '.') {
			state = transition_table[state][0];
		} else if (morse->at[i] == '_') {
			state = transition_table[state][1];
		} else if (morse->at[i] == ' ') {
			if (state < 5) state = '?';
			if (state == 5) { //SOS
				write_SOS(text);
			} else {
				text->at[text->len++] = state;
			}
			state = '\0';
		} else { //== '/'
			if (state < 5) state = '?';
			if (state == 5) { //SOS
				write_SOS(text);
			} else {
				text->at[text->len++] = state;
			}
			text->at[text->len++] = ' ';
			state = '\0';
		}
	}
	if (state != '\0') {
		if (state < 5) state = '?';
		if (state == 5) { //SOS
			write_SOS(text);
		} else {
			text->at[text->len++] = state;
		}
	}
	text->at[text->len] = '\0';
}

int check_danger(String msg) {
	if (
		msg.indexOf("SOS") != -1 || msg.indexOf("S O S") != -1 ||
		msg.indexOf("S OS") != -1 || msg.indexOf("SO S") != -1
	) {
		return 1;
	}
	return 0;
}

void print_intervals() {
	for (int i = 0; i < intervals.len; i++) {
		Serial.print(intervals.at[i]);
		Serial.print(' ');
	}
	Serial.println("");
}

void print_morse() {
	for (int i = 0; i < morse.len; i++) {
		Serial.print(morse.at[i]);
	}
	Serial.println("");
}

void print_text() {
	for (int i = 0; i < text.len; i++) {
		Serial.print(text.at[i]);
	}
	Serial.println("");
}

int emit_event(String id, String message) {
	HTTPClient http;
	http.begin(String(SERVER_URL) + "/events/emit");
	http.addHeader("Content-Type", "application/json");
	DynamicJsonDocument doc(1024); //should be static
	doc["id"] = id;
	doc["message"] = message;
	doc["timestamp"] = fetch_timestamp();
	String body;
	serializeJson(doc, body);
	Serial.println(body);
	int status = http.POST(body);
	http.end();
	return status;
}

String fetch_timestamp() {
	HTTPClient http;
	http.begin("http://worldtimeapi.org/api/timezone/America/Fortaleza");
	int status = http.GET();
	if (status == -1)
		return "bruh";
	String payload = http.getString();
	http.end();
	DynamicJsonDocument doc(1024);
	deserializeJson(doc, payload);
	return doc["datetime"];
}
