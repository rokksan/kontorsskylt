#include <Arduino.h>
#include <SPI.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>


// ============================================================
// SVENSKA TECKEN
// ============================================================

enum SwedishMark
{
    MARK_NONE,
    MARK_RING,
    MARK_DOTS
};

struct DisplayCharacter
{
    char base;
    SwedishMark mark;
};


// ============================================================
// KOMMANDOTYP
// ============================================================

enum CommandType
{
    COMMAND_NONE,
    COMMAND_OFFICE,
    COMMAND_HOME,
    COMMAND_CUSTOM
};


// ============================================================
// E-PAPER PINS
// ============================================================

#define EPD_CS   D7
#define EPD_DC   D3
#define EPD_RST  D2
#define EPD_BUSY D1


// ============================================================
// DISPLAY
// ============================================================

GxEPD2_3C<GxEPD2_213_Z98c, GxEPD2_213_Z98c::HEIGHT> display(
    GxEPD2_213_Z98c(
        EPD_CS,
        EPD_DC,
        EPD_RST,
        EPD_BUSY
    )
);


// ============================================================
// AKTUELL FONT
// ============================================================

const GFXfont* currentFont = nullptr;


void selectFont(
    const GFXfont* font
)
{
    currentFont = font;
    display.setFont(font);
}


// ============================================================
// BLE
// ============================================================

#define SERVICE_UUID \
    "7d230001-5475-4a28-a975-8a03cafe0001"

#define CHARACTERISTIC_UUID \
    "7d230002-5475-4a28-a975-8a03cafe0001"


BLECharacteristic* statusCharacteristic;


// ============================================================
// VÄNTANDE DISPLAYKOMMANDO
//
// BLE-callbacken skriver hit.
// loop() läser härifrån och uppdaterar displayen.
//
// Eftersom vi bara tillåter ett väntande kommando åt gången
// slipper vi köra e-paper inne i BLE-callbacken.
// ============================================================

volatile bool commandPending = false;

CommandType pendingCommand =
    COMMAND_NONE;

String pendingText = "";

uint16_t pendingBackgroundColor =
    GxEPD_WHITE;

uint16_t pendingForegroundColor =
    GxEPD_BLACK;


// ============================================================
// LÄS ETT UTF-8-TECKEN
// ============================================================

DisplayCharacter readDisplayCharacter(
    const String& text,
    int& index
)
{
    DisplayCharacter result;

    result.base = '?';
    result.mark = MARK_NONE;


    uint8_t c =
        (uint8_t)text[index];


    // ASCII
    if (c < 0x80)
    {
        result.base =
            (char)c;

        index++;

        return result;
    }


    // Svenska UTF-8-tecken
    if (
        c == 0xC3 &&
        index + 1 < text.length()
    )
    {
        uint8_t next =
            (uint8_t)text[index + 1];


        // å
        if (next == 0xA5)
        {
            result.base = 'a';
            result.mark = MARK_RING;
        }

        // ä
        else if (next == 0xA4)
        {
            result.base = 'a';
            result.mark = MARK_DOTS;
        }

        // ö
        else if (next == 0xB6)
        {
            result.base = 'o';
            result.mark = MARK_DOTS;
        }

        // Å
        else if (next == 0x85)
        {
            result.base = 'A';
            result.mark = MARK_RING;
        }

        // Ä
        else if (next == 0x84)
        {
            result.base = 'A';
            result.mark = MARK_DOTS;
        }

        // Ö
        else if (next == 0x96)
        {
            result.base = 'O';
            result.mark = MARK_DOTS;
        }

        else
        {
            result.base = '?';
        }


        index += 2;

        return result;
    }


    // Okänt UTF-8
    result.base = '?';

    index++;

    return result;
}


// ============================================================
// FONTENS VERKLIGA X-ADVANCE
// ============================================================

int getGlyphAdvance(
    char c
)
{
    if (currentFont == nullptr)
    {
        return 6;
    }


    uint8_t uc =
        (uint8_t)c;


    if (
        uc < currentFont->first ||
        uc > currentFont->last
    )
    {
        uc = '?';
    }


    const GFXglyph* glyph =
        currentFont->glyph +
        (uc - currentFont->first);


    return glyph->xAdvance;
}


// ============================================================
// MÄT TEXT MED X-ADVANCE
// ============================================================

int measureSwedishText(
    const String& text
)
{
    int width = 0;
    int index = 0;


    while (index < text.length())
    {
        DisplayCharacter ch =
            readDisplayCharacter(
                text,
                index
            );


        width +=
            getGlyphAdvance(
                ch.base
            );
    }


    return width;
}


// ============================================================
// RITA SVENSK MARKERING
//
// Å/å använder den fyllda markering som redan verifierats.
//
// Ä/Ö/ä/ö använder två fyllda prickar.
// ============================================================

void drawSwedishMark(
    int charX,
    int baselineY,
    char base,
    SwedishMark mark,
    uint16_t color
)
{
    if (mark == MARK_NONE)
    {
        return;
    }


    char temp[2];

    temp[0] = base;
    temp[1] = '\0';


    int16_t x1;
    int16_t y1;

    uint16_t w;
    uint16_t h;


    display.getTextBounds(
        temp,
        charX,
        baselineY,
        &x1,
        &y1,
        &w,
        &h
    );


    int centerX =
        x1 + (w / 2);

    int topY =
        y1;


    // å / Å
    if (mark == MARK_RING)
    {
        int markY =
            topY - 4;


        display.fillCircle(
            centerX,
            markY,
            2,
            color
        );
    }


    // ä / ö / Ä / Ö
    else if (mark == MARK_DOTS)
    {
        int markY =
            topY - 3;


        display.fillCircle(
            centerX - 4,
            markY,
            2,
            color
        );


        display.fillCircle(
            centerX + 4,
            markY,
            2,
            color
        );
    }
}


// ============================================================
// RITA CENTRERAD SVENSK TEXT
// ============================================================

void drawSwedishCentered(
    const String& text,
    int baselineY,
    uint16_t color
)
{
    int textWidth =
        measureSwedishText(
            text
        );


    int startX =
        (
            display.width()
            - textWidth
        ) / 2;


    display.setCursor(
        startX,
        baselineY
    );


    int index = 0;


    while (index < text.length())
    {
        DisplayCharacter ch =
            readDisplayCharacter(
                text,
                index
            );


        int charX =
            display.getCursorX();


        display.print(
            ch.base
        );


        drawSwedishMark(
            charX,
            baselineY,
            ch.base,
            ch.mark,
            color
        );
    }
}


// ============================================================
// RADBRYTNING
//
// 18 pt -> max 1 rad
// 12 pt -> max 2 rader
//  9 pt -> max 3 rader
//
// Samma verifierade xAdvance-mätning används.
// ============================================================

int wrapText(
    const String& text,
    String lines[],
    int maxLines,
    int maxWidth
)
{
    for (
        int i = 0;
        i < 3;
        i++
    )
    {
        lines[i] = "";
    }


    int lineCount = 0;

    String currentLine = "";
    String word = "";

    int position = 0;


    while (position <= text.length())
    {
        char c;


        if (position < text.length())
        {
            c =
                text[position];
        }
        else
        {
            c = ' ';
        }


        if (
            c == ' ' ||
            c == '\n'
        )
        {
            if (word.length() > 0)
            {
                String candidate;


                if (
                    currentLine.length() == 0
                )
                {
                    candidate =
                        word;
                }
                else
                {
                    candidate =
                        currentLine
                        + " "
                        + word;
                }


                if (
                    measureSwedishText(
                        candidate
                    )
                    <= maxWidth
                )
                {
                    currentLine =
                        candidate;
                }

                else
                {
                    if (
                        currentLine.length() == 0
                    )
                    {
                        return -1;
                    }


                    if (
                        lineCount >= maxLines
                    )
                    {
                        return -1;
                    }


                    lines[lineCount] =
                        currentLine;

                    lineCount++;


                    currentLine =
                        word;


                    if (
                        measureSwedishText(
                            currentLine
                        )
                        > maxWidth
                    )
                    {
                        return -1;
                    }
                }


                word = "";
            }


            if (
                c == '\n' &&
                currentLine.length() > 0
            )
            {
                if (
                    lineCount >= maxLines
                )
                {
                    return -1;
                }


                lines[lineCount] =
                    currentLine;

                lineCount++;


                currentLine = "";
            }
        }

        else
        {
            word += c;
        }


        position++;
    }


    if (
        currentLine.length() > 0
    )
    {
        if (
            lineCount >= maxLines
        )
        {
            return -1;
        }


        lines[lineCount] =
            currentLine;

        lineCount++;
    }


    return lineCount;
}


// ============================================================
// OFFICE
// ============================================================

void showOffice()
{
    Serial.println(
        "Updating display: OFFICE"
    );


    display.setRotation(1);
    display.setFullWindow();


    display.firstPage();


    do
    {
        display.fillScreen(
            GxEPD_WHITE
        );


        display.setTextColor(
            GxEPD_BLACK
        );


        selectFont(
            &FreeMonoBold18pt7b
        );


        drawSwedishCentered(
            "PÅ KONTORET",
            78,
            GxEPD_BLACK
        );
    }
    while (
        display.nextPage()
    );


    display.hibernate();


    Serial.println(
        "Display update complete."
    );
}


// ============================================================
// HOME
// ============================================================

void showHome()
{
    Serial.println(
        "Updating display: HOME"
    );


    display.setRotation(1);
    display.setFullWindow();


    display.firstPage();


    do
    {
        display.fillScreen(
            GxEPD_RED
        );


        display.setTextColor(
            GxEPD_WHITE
        );


        selectFont(
            &FreeMonoBold12pt7b
        );


        drawSwedishCentered(
            "ARBETAR",
            52,
            GxEPD_WHITE
        );


        drawSwedishCentered(
            "HEMIFRÅN",
            88,
            GxEPD_WHITE
        );
    }
    while (
        display.nextPage()
    );


    display.hibernate();


    Serial.println(
        "Display update complete."
    );
}


// ============================================================
// CUSTOM
// ============================================================

bool showCustom(
    const String& text,
    uint16_t backgroundColor,
    uint16_t foregroundColor
)
{
    Serial.println(
        "Updating display: CUSTOM"
    );


    Serial.print(
        "Text: "
    );


    Serial.println(
        text
    );


    display.setRotation(1);
    display.setFullWindow();


    String lines[3];

    int lineCount =
        -1;

    int lineHeight =
        0;

    int maxWidth =
        display.width() - 20;


    // ========================================================
    // 18 PT - MAX 1 RAD
    // ========================================================

    selectFont(
        &FreeMonoBold18pt7b
    );


    lineCount =
        wrapText(
            text,
            lines,
            1,
            maxWidth
        );


    if (lineCount > 0)
    {
        lineHeight = 38;


        Serial.println(
            "Layout: 18 pt"
        );
    }


    // ========================================================
    // 12 PT - MAX 2 RADER
    // ========================================================

    if (lineCount < 1)
    {
        selectFont(
            &FreeMonoBold12pt7b
        );


        lineCount =
            wrapText(
                text,
                lines,
                2,
                maxWidth
            );


        if (lineCount > 0)
        {
            lineHeight = 28;


            Serial.println(
                "Layout: 12 pt"
            );
        }
    }


    // ========================================================
    // 9 PT - MAX 3 RADER
    // ========================================================

    if (lineCount < 1)
    {
        selectFont(
            &FreeMonoBold9pt7b
        );


        lineCount =
            wrapText(
                text,
                lines,
                3,
                maxWidth
            );


        if (lineCount > 0)
        {
            lineHeight = 22;


            Serial.println(
                "Layout: 9 pt"
            );
        }
    }


    // ========================================================
    // FÅR INTE PLATS
    // ========================================================

    if (lineCount < 1)
    {
        Serial.println(
            "ERROR: Text does not fit display."
        );


        return false;
    }


    // ========================================================
    // VERTIKAL CENTRERING
    // ========================================================

    int totalHeight =
        lineCount * lineHeight;


    int firstBaseline =
        (
            display.height()
            - totalHeight
        ) / 2
        + lineHeight
        - 5;


    // ========================================================
    // RITA
    // ========================================================

    display.firstPage();


    do
    {
        display.fillScreen(
            backgroundColor
        );


        display.setTextColor(
            foregroundColor
        );


        for (
            int i = 0;
            i < lineCount;
            i++
        )
        {
            int baseline =
                firstBaseline
                + i * lineHeight;


            drawSwedishCentered(
                lines[i],
                baseline,
                foregroundColor
            );
        }
    }
    while (
        display.nextPage()
    );


    display.hibernate();


    Serial.print(
        "CUSTOM lines: "
    );


    Serial.println(
        lineCount
    );


    Serial.println(
        "Custom display update complete."
    );


    return true;
}


// ============================================================
// SKICKA STATUS
// ============================================================

void sendStatus(
    const char* status
)
{
    if (
        statusCharacteristic == nullptr
    )
    {
        return;
    }


    statusCharacteristic->setValue(
        status
    );


    // notify() får gärna anropas även om ingen klient längre
    // lyssnar. Det viktiga är att displayjobbet inte längre
    // körs från onWrite().
    statusCharacteristic->notify();


    Serial.print(
        "BLE response: "
    );


    Serial.println(
        status
    );
}


// ============================================================
// KÖA KOMMANDO
//
// Endast ett displayjobb åt gången.
//
// Om displayen redan har ett väntande jobb svarar vi BUSY.
// Det är bättre än att skriva över ett kommando tyst.
// ============================================================

bool queueCommand(
    CommandType command,
    const String& text,
    uint16_t backgroundColor,
    uint16_t foregroundColor
)
{
    if (commandPending)
    {
        return false;
    }


    pendingCommand =
        command;


    pendingText =
        text;


    pendingBackgroundColor =
        backgroundColor;


    pendingForegroundColor =
        foregroundColor;


    // Sätt denna sist, när all data är färdigkopierad.
    commandPending =
        true;


    return true;
}


// ============================================================
// BLE CHARACTERISTIC CALLBACK
//
// VIKTIG ÄNDRING:
//
// INGEN e-paper-uppdatering sker här.
//
// Callbacken:
// 1. tar emot
// 2. validerar
// 3. köar
// 4. returnerar snabbt
// ============================================================

class StatusCallbacks :
    public BLECharacteristicCallbacks
{
    void onWrite(
        BLECharacteristic* characteristic
    )
    {
        String value =
            characteristic
                ->getValue()
                .c_str();


        value.trim();


        String commandUpper =
            value;


        commandUpper.toUpperCase();


        Serial.print(
            "Received BLE command: "
        );


        Serial.println(
            value
        );


        // ====================================================
        // OFFICE
        // ====================================================

        if (
            commandUpper == "OFFICE"
        )
        {
            if (
                !queueCommand(
                    COMMAND_OFFICE,
                    "",
                    GxEPD_WHITE,
                    GxEPD_BLACK
                )
            )
            {
                characteristic->setValue(
                    "ERR|BUSY"
                );


                characteristic->notify();


                Serial.println(
                    "BLE response: ERR|BUSY"
                );
            }

            else
            {
                characteristic->setValue(
                    "OK|QUEUED"
                );


                characteristic->notify();


                Serial.println(
                    "BLE response: OK|QUEUED"
                );
            }


            return;
        }


        // ====================================================
        // HOME
        // ====================================================

        if (
            commandUpper == "HOME"
        )
        {
            if (
                !queueCommand(
                    COMMAND_HOME,
                    "",
                    GxEPD_RED,
                    GxEPD_WHITE
                )
            )
            {
                characteristic->setValue(
                    "ERR|BUSY"
                );


                characteristic->notify();


                Serial.println(
                    "BLE response: ERR|BUSY"
                );
            }

            else
            {
                characteristic->setValue(
                    "OK|QUEUED"
                );


                characteristic->notify();


                Serial.println(
                    "BLE response: OK|QUEUED"
                );
            }


            return;
        }


        // ====================================================
        // CUSTOM
        // ====================================================

        if (
            commandUpper.startsWith(
                "CUSTOM|"
            )
        )
        {
            int bgPos =
                commandUpper.indexOf(
                    "BG="
                );


            int fgPos =
                commandUpper.indexOf(
                    "|FG="
                );


            int textPos =
                commandUpper.indexOf(
                    "|TEXT="
                );


            if (
                bgPos == -1 ||
                fgPos == -1 ||
                textPos == -1
            )
            {
                characteristic->setValue(
                    "ERR|INVALID_COMMAND"
                );


                characteristic->notify();


                Serial.println(
                    "BLE response: ERR|INVALID_COMMAND"
                );


                return;
            }


            String bg =
                commandUpper.substring(
                    bgPos + 3,
                    fgPos
                );


            String fg =
                commandUpper.substring(
                    fgPos + 4,
                    textPos
                );


            // Originaltexten bevaras.
            String text =
                value.substring(
                    textPos + 6
                );


            // =================================================
            // VALIDERING
            // =================================================

            if (
                text.length() == 0
            )
            {
                characteristic->setValue(
                    "ERR|EMPTY_TEXT"
                );


                characteristic->notify();


                Serial.println(
                    "BLE response: ERR|EMPTY_TEXT"
                );


                return;
            }


            if (
                text.length() > 120
            )
            {
                characteristic->setValue(
                    "ERR|TOO_LONG"
                );


                characteristic->notify();


                Serial.println(
                    "BLE response: ERR|TOO_LONG"
                );


                return;
            }


            if (
                bg == fg
            )
            {
                characteristic->setValue(
                    "ERR|SAME_COLOR"
                );


                characteristic->notify();


                Serial.println(
                    "BLE response: ERR|SAME_COLOR"
                );


                return;
            }


            // =================================================
            // BAKGRUNDSFÄRG
            // =================================================

            uint16_t backgroundColor;


            if (
                bg == "WHITE"
            )
            {
                backgroundColor =
                    GxEPD_WHITE;
            }

            else if (
                bg == "BLACK"
            )
            {
                backgroundColor =
                    GxEPD_BLACK;
            }

            else if (
                bg == "RED"
            )
            {
                backgroundColor =
                    GxEPD_RED;
            }

            else
            {
                characteristic->setValue(
                    "ERR|INVALID_COLOR"
                );


                characteristic->notify();


                Serial.println(
                    "BLE response: ERR|INVALID_COLOR"
                );


                return;
            }


            // =================================================
            // TEXTFÄRG
            // =================================================

            uint16_t foregroundColor;


            if (
                fg == "WHITE"
            )
            {
                foregroundColor =
                    GxEPD_WHITE;
            }

            else if (
                fg == "BLACK"
            )
            {
                foregroundColor =
                    GxEPD_BLACK;
            }

            else if (
                fg == "RED"
            )
            {
                foregroundColor =
                    GxEPD_RED;
            }

            else
            {
                characteristic->setValue(
                    "ERR|INVALID_COLOR"
                );


                characteristic->notify();


                Serial.println(
                    "BLE response: ERR|INVALID_COLOR"
                );


                return;
            }


            // =================================================
            // KÖA CUSTOM
            // =================================================

            if (
                !queueCommand(
                    COMMAND_CUSTOM,
                    text,
                    backgroundColor,
                    foregroundColor
                )
            )
            {
                characteristic->setValue(
                    "ERR|BUSY"
                );


                characteristic->notify();


                Serial.println(
                    "BLE response: ERR|BUSY"
                );
            }

            else
            {
                characteristic->setValue(
                    "OK|QUEUED"
                );


                characteristic->notify();


                Serial.println(
                    "BLE response: OK|QUEUED"
                );
            }


            return;
        }


        // ====================================================
        // OKÄNT KOMMANDO
        // ====================================================

        characteristic->setValue(
            "ERR|INVALID_COMMAND"
        );


        characteristic->notify();


        Serial.println(
            "BLE response: ERR|INVALID_COMMAND"
        );
    }
};


// ============================================================
// BLE SERVER CALLBACK
// ============================================================

class ServerCallbacks :
    public BLEServerCallbacks
{
    void onConnect(
        BLEServer* server
    )
    {
        Serial.println(
            "BLE client connected."
        );
    }


    void onDisconnect(
        BLEServer* server
    )
    {
        Serial.println(
            "BLE client disconnected."
        );


        // Ingen lång displayuppdatering pågår längre inne
        // i BLE-callbacken.
        delay(
            100
        );


        BLEDevice::startAdvertising();


        Serial.println(
            "BLE advertising restarted."
        );
    }
};


// ============================================================
// BEHANDLA VÄNTANDE KOMMANDO
//
// Körs från loop(), alltså utanför BLE onWrite().
// ============================================================

void processPendingCommand()
{
    if (!commandPending)
    {
        return;
    }


    // Kopiera all data lokalt innan vi börjar den långsamma
    // e-paper-uppdateringen.
    CommandType command =
        pendingCommand;


    String text =
        pendingText;


    uint16_t backgroundColor =
        pendingBackgroundColor;


    uint16_t foregroundColor =
        pendingForegroundColor;


    // Markera jobbet som hämtat.
    //
    // Ett nytt BLE-kommando kan därmed tas emot medan
    // displayen arbetar. Det blir i så fall nästa väntande jobb.
    commandPending =
        false;


    pendingCommand =
        COMMAND_NONE;


    pendingText =
        "";


    // ========================================================
    // OFFICE
    // ========================================================

    if (
        command == COMMAND_OFFICE
    )
    {
        showOffice();


        sendStatus(
            "OK|OFFICE"
        );


        return;
    }


    // ========================================================
    // HOME
    // ========================================================

    if (
        command == COMMAND_HOME
    )
    {
        showHome();


        sendStatus(
            "OK|HOME"
        );


        return;
    }


    // ========================================================
    // CUSTOM
    // ========================================================

    if (
        command == COMMAND_CUSTOM
    )
    {
        bool success =
            showCustom(
                text,
                backgroundColor,
                foregroundColor
            );


        if (success)
        {
            sendStatus(
                "OK|CUSTOM"
            );
        }

        else
        {
            sendStatus(
                "ERR|TEXT_DOES_NOT_FIT"
            );
        }


        return;
    }
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(
        115200
    );


    delay(
        1000
    );


    Serial.println();


    Serial.println(
        "=============================="
    );


    Serial.println(
        "Kontorsskylt starting..."
    );


    Serial.println(
        "=============================="
    );


    // ========================================================
    // SPI
    //
    // XIAO ESP32-C3:
    //
    // D8  = SCK
    // D9  = MISO
    // D10 = MOSI
    // D7  = CS
    // ========================================================

    SPI.begin(
        D8,
        D9,
        D10,
        EPD_CS
    );


    display.epd2.selectSPI(
        SPI,

        SPISettings(
            4000000,
            MSBFIRST,
            SPI_MODE0
        )
    );


    // ========================================================
    // DISPLAY
    // ========================================================

    Serial.println(
        "Initializing e-paper..."
    );


    display.init(
        115200,
        true,
        2,
        false
    );


    // ========================================================
    // STARTBILD
    // ========================================================

    Serial.println(
        "Showing initial OFFICE screen..."
    );


    showOffice();


    // ========================================================
    // BLE
    // ========================================================

    Serial.println(
        "Starting BLE..."
    );


    BLEDevice::init(
        "Kontorsskylt"
    );


    BLEServer* server =
        BLEDevice::createServer();


    server->setCallbacks(
        new ServerCallbacks()
    );


    BLEService* service =
        server->createService(
            SERVICE_UUID
        );


    statusCharacteristic =
        service->createCharacteristic(
            CHARACTERISTIC_UUID,

            BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_NOTIFY
        );


    statusCharacteristic->setCallbacks(
        new StatusCallbacks()
    );


    statusCharacteristic->setValue(
        "OK|OFFICE"
    );


    service->start();


    // ========================================================
    // ADVERTISING
    // ========================================================

    BLEAdvertising* advertising =
        BLEDevice::getAdvertising();


    advertising->addServiceUUID(
        SERVICE_UUID
    );


    advertising->setScanResponse(
        true
    );


    BLEDevice::startAdvertising();


    // ========================================================
    // READY
    // ========================================================

    Serial.println();


    Serial.println(
        "=============================="
    );


    Serial.println(
        "Kontorsskylt BLE started."
    );


    Serial.println(
        "Device name: Kontorsskylt"
    );


    Serial.println(
        "=============================="
    );
}


// ============================================================
// LOOP
//
// Här sker nu e-paper-arbetet.
// ============================================================

void loop()
{
    processPendingCommand();

    delay(
        10
    );
}