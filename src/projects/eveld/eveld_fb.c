#include "eveld.h"

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))


void PrepareScreen() {
    Send_CMD(CMD_DLSTART);
    Send_CMD(VERTEXFORMAT(0));
    Send_CMD(CLEAR_COLOR_RGB(0, 0, 0));
    Send_CMD(CLEAR(1, 1, 1));
}

void DisplayFrame() {
    Send_CMD(DISPLAY());
    Send_CMD(CMD_SWAP);
    UpdateFIFO();
    Wait4CoProFIFOEmpty();
}

void ClearScreen(void) {
    DisplayFrame();
    staticTextCount = 0;
}

void ResetScreen(void) {
    ClearScreen();
    PrepareScreen();
}


int GetCharWidth(uint16_t font_size, char ch) {
    int baseWidth = font_size * 0.5;
    
    switch (ch) {
        case 'M': return baseWidth * 1.4;
        case 'W': return baseWidth * 1.5;
        case 'I': return baseWidth * 0.1;
        case 'i': case 'l': case '!': case '\'': case '"':
            return baseWidth * 0.8;
        case 'f': case 't': case 'j': return baseWidth * 0.9;
        case 'b': case 'p': case 'v': case 'm': case 'w': return baseWidth * 1.2;
        case '0': return baseWidth * 1.2;
    }
    
    if (ch >= 'A' && ch <= 'Z') return baseWidth * 1.4;
    
    return baseWidth;
}

int GetFontHeight(int font) {
    return font;
}

int GetTextWidth(const char* text, int font) {
    int width = 0;
    while (*text) {
        width += GetCharWidth(font, *text);
        text++;
    }
    return width;
}


bool is_valid_utf8(const char **ptr) {
    const unsigned char *bytes = (const unsigned char *)*ptr;

    if (bytes[0] == 'M' && bytes[1] == '-' && bytes[2] == 'b') {
        *ptr += 10;
        return false;
    }

    if (bytes[0] <= 0x7F) {
        // 1-byte character (ASCII)
        return true;
    } else if ((bytes[0] & 0xE0) == 0xC0) {
        // 2-byte character
        if ((bytes[1] & 0xC0) == 0x80) {
            *ptr += 1;
            return true;
        }
    } else if ((bytes[0] & 0xF0) == 0xE0) {
        // 3-byte character
        if ((bytes[1] & 0xC0) == 0x80 && (bytes[2] & 0xC0) == 0x80) {
            *ptr += 2;
            return true;
        }
    } else if ((bytes[0] & 0xF8) == 0xF0) {
        // 4-byte character
        if ((bytes[1] & 0xC0) == 0x80 && (bytes[2] & 0xC0) == 0x80 && (bytes[3] & 0xC0) == 0x80) {
            *ptr += 3;
            return true;
        }
    }

    *ptr += 1;
    return false;
}


bool colors_are_equal(Color a, Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

void SetActualNewLine(uint16_t line) {
    DEBUG_PRINT("New line, y = %d x = %d\n", actual_word.y, actual_word.x);
    actual_word.line = line;
    mutex_newline = true;
}


void AppendCharToActualWord(char ch) {
    if (actual_word_len < MAX_LENGTH - 1) {
        actual_word.text[actual_word_len] = ch;
        actual_word_len++;
        actual_word.text[actual_word_len] = '\0';
        actual_word.width += GetCharWidth(actual_word.font, ch);
    } else {
        ERROR_PRINT("Error during append char: maximum length reached\n");
    }
}

void AddActualTextStatic(void) {
    if (actual_word_len <= 0) {
        return;
    }

    if (staticTextCount >= MAX_STATIC_TEXTS) {
        DEBUG_PRINT("Maximum number of static texts reached\n");
        ClearLine();
        return;
    }

    if (mutex_newline) {
        //DEBUG_PRINT("Clear mutex is on!\n");
        mutex_newline = false;
    }

    if (actual_word.font > 32 || actual_word.font < 15) {
        ERROR_PRINT("Error during add static text: invalid font size\n");
        return;
    }

    if (actual_word.y < 0) {
        DEBUG_PRINT("Y position is too low, setting to 0\n");
        actual_word.y = 0;
    }

    if (actual_word.y + GetFontHeight(actual_word.font) >= Display_Height()) {
        actual_word.y = Display_Height() - GetFontHeight(actual_word.font);
        DEBUG_PRINT("Y position is too high, setting to %d\n", actual_word.y);
    }

    if (actual_word.x < 0) {
        actual_word.x = 0;
    }

    if (actual_word.x + actual_word.width >= Display_Width()) {
        actual_word.x = Display_Width() - actual_word.width;
    }

    DEBUG_PRINT("Adding word: '%s' with width %d fg color " COLOR_FMT " and bg color " COLOR_FMT " at %d, %d position\n", 
        actual_word.text, 
        actual_word.width,
        COLOR_ARGS(actual_word.text_color),
        COLOR_ARGS(actual_word.bg_color),
        actual_word.x, actual_word.y
    );

    actual_word.text[actual_word_len] = '\0';

    staticTexts[staticTextCount++] = actual_word;

    actual_word.x += actual_word.width;
    actual_word_len = 0;
    actual_word.width = 0;
}


void DrawStaticTexts(void) {
    for (int i = 0; i < staticTextCount; i++) {

        if (!colors_are_equal(staticTexts[i].bg_color, 
            (Color){DEFAULT_COLOR_BG_R, DEFAULT_COLOR_BG_G, DEFAULT_COLOR_BG_B})) {

            Send_CMD(COLOR_RGB(
                staticTexts[i].bg_color.r,
                staticTexts[i].bg_color.g,
                staticTexts[i].bg_color.b
            ));

            Send_CMD(BEGIN(RECTS));
            Send_CMD(VERTEX2F(
                staticTexts[i].x, 
                staticTexts[i].y
            ));
            Send_CMD(VERTEX2F(
                staticTexts[i].x + GetTextWidth(staticTexts[i].text, staticTexts[i].font) - 5, 
                staticTexts[i].y + GetFontHeight(staticTexts[i].font) - 10
            ));
            Send_CMD(END());
        }

        Send_CMD(COLOR_RGB(
            staticTexts[i].text_color.r,
            staticTexts[i].text_color.g,
            staticTexts[i].text_color.b
        ));
        
        Cmd_Text(
            staticTexts[i].x, staticTexts[i].y, 
            staticTexts[i].font, DEFAULT_OPTION, 
            staticTexts[i].text
        );
    }

    DEBUG_PRINT("staticTextCount: %d\n", staticTextCount);
    DisplayFrame();
}


void DeleteChatH(uint16_t count) {
    for (int i = 0; i < staticTextCount; i++) {
        if (staticTexts[i].line != actual_word.line || staticTexts[i].x <= actual_word.x) {
            staticTexts[i].x -= count*GetCharWidth(staticTexts[i].font, ' ');
        }
    }
} 


void ClearLine(void) {
    int j = 0;
    for (int i = 0; i < staticTextCount; i++) {
        if (staticTexts[i].line != actual_word.line) {
            staticTexts[j++] = staticTexts[i];
        } else {
            DEBUG_PRINT("Cleared Line %d: %s\n", staticTexts[i].line, staticTexts[i].text);
        }
    }
    staticTextCount = j;
}


void ClearLineAfterX(void) {
    int j = 0;
    for (int i = 0; i < staticTextCount; i++) {
        if (staticTexts[i].line != actual_word.line || staticTexts[i].x <= actual_word.x) {
            staticTexts[j++] = staticTexts[i];
        } else {
            DEBUG_PRINT("Cleared After X=%d: %s\n", actual_word.x, staticTexts[i].text);
        }
    }
    staticTextCount = j;
}

void ClearLineBeforeX(void) {
    int j = 0;
    for (int i = 0; i < staticTextCount; i++) {
        if (staticTexts[i].line != actual_word.line || staticTexts[i].x > actual_word.x) {
            staticTexts[j++] = staticTexts[i];
        } else {
            DEBUG_PRINT("Cleared Before X=%d: %s\n", actual_word.x, staticTexts[i].text);
        }
    }
    staticTextCount = j;
}


void ClearPlaceForActual(void) {
    int j = 0;
    for (int i = 0; i < staticTextCount; i++) {
        if (staticTexts[i].line != actual_word.line || 
            (staticTexts[i].x + staticTexts[i].width <= actual_word.x || 
             staticTexts[i].x >= actual_word.x + actual_word.width)) {
            staticTexts[j++] = staticTexts[i];
        } else {
            DEBUG_PRINT("Cleared Place X=%d: %s\n", actual_word.x, staticTexts[i].text);
        }
    }
    staticTextCount = j;
}


int GetTextOffset(StaticText *word, int xPos) {
    int offset = 0, px = word->x;
    while (px < xPos && word->text[offset] != '\0') {
        int charWidth = GetCharWidth(word->font, word->text[offset]);
        if (px + charWidth > xPos) {
            break;
        }
        px += charWidth;
        offset++;
    }
    return offset;
}

StaticText CreateSubText(StaticText src, int newX, int newWidth) {
    StaticText subText = src;
    subText.x = newX;
    subText.width = newWidth;
    
    int charOffset = GetTextOffset(&src, newX);
    if (charOffset >= strlen(src.text)) {
        subText.text[0] = '\0';
        return subText;
    }
    
    int px = newX;
    int textLen = 0;
    for (int i = charOffset; src.text[i] != '\0' && px < newX + newWidth; i++) {
        int charWidth = GetCharWidth(src.font, src.text[i]);
        if (px + charWidth > newX + newWidth) {
            break;
        }
        subText.text[textLen++] = src.text[i];
        px += charWidth;
    }
    subText.text[textLen] = '\0';
    
    if (textLen == 0) {
        subText.text[0] = '\0';
    }
    return subText;
}

bool IsOnlySpaces(const char *text) {
    for (int i = 0; text[i] != '\0'; i++) {
        if (text[i] != ' ') {
            return false;
        }
    }
    return true;
}

void TrimTrailingSpaces(char *text) {
    int len = strlen(text);
    while (len > 0 && text[len - 1] == ' ') {
        text[--len] = '\0';
    }
}

void AddOrMergeActualTextStatic(void) {
    if (actual_word_len <= 0) {
        DEBUG_PRINT("Skipping empty text\n");
        return;
    }
    actual_word.text[actual_word_len] = '\0';

    if (IsOnlySpaces(actual_word.text)) {
        DEBUG_PRINT("Text contains only spaces, moving x to %d\n", actual_word.x + actual_word.width);
        actual_word.x += actual_word.width;
        actual_word_len = 0;
        actual_word.width = 0;
        return;
    }

    if (actual_word.font > 32 || actual_word.font < 15) {
        ERROR_PRINT("Error during add static text: invalid font size %d\n", actual_word.font);
        return;
    }

    actual_word.y = max(0, actual_word.y);
    actual_word.y = min(actual_word.y, Display_Height() - GetFontHeight(actual_word.font));
    actual_word.x = max(0, actual_word.x);
    actual_word.width = min(actual_word.width, Display_Width() - actual_word.x);

    DEBUG_PRINT("Adding word: '%s' at [%d, %d] width: %d, font: %d\n", 
        actual_word.text, actual_word.x, actual_word.y, actual_word.width, actual_word.font);

    StaticText newStaticTexts[MAX_STATIC_TEXTS];
    int newCount = 0;
    bool merged = false;
    StaticText *intersectingWords[MAX_STATIC_TEXTS];
    int intersectCount = 0;

    for (int i = 0; i < staticTextCount; i++) {
        StaticText *word = &staticTexts[i];

        if (word->line != actual_word.line || word->x + word->width < actual_word.x || word->x > actual_word.x + actual_word.width) {
            newStaticTexts[newCount++] = *word;
        } else {
            DEBUG_PRINT("Intersecting word: '%s' at [%d, %d] width: %d\n", word->text, word->x, word->y, word->width);
            intersectingWords[intersectCount++] = word;
        }
    }

    if (intersectCount == 0) {
        DEBUG_PRINT("No intersections, adding new word: '%s'\n", actual_word.text);
        newStaticTexts[newCount++] = actual_word;
    } else {
        for (int i = 0; i < intersectCount; i++) {
            StaticText *word = intersectingWords[i];

            if (word->font == actual_word.font &&
                colors_are_equal(word->text_color, actual_word.text_color) &&
                colors_are_equal(word->bg_color, actual_word.bg_color)) {
            
                DEBUG_PRINT("Merging text '%s' with '%s'\n", word->text, actual_word.text);
            
                int char_width = GetCharWidth(word->font, ' ');
                int actual_start = max(0, (actual_word.x - word->x) / char_width);
                int actual_len = strlen(actual_word.text);
            
                if (actual_start + actual_len > MAX_LENGTH) {
                    ERROR_PRINT("Text merge out of bounds\n");
                    return;
                }
            
                // Очищаем старый текст перед заменой (убираем артефакты)
                memset(word->text + actual_start, ' ', actual_len);
                memcpy(word->text + actual_start, actual_word.text, actual_len);
                word->text[max(actual_start + actual_len, strlen(word->text))] = '\0';
            
                word->x = min(word->x, actual_word.x);
                word->width = (strlen(word->text) * char_width); // Учитываем реальный размер текста
            
                DEBUG_PRINT("Merged result: '%s' at [%d, %d] width: %d\n", word->text, word->x, word->y, word->width);
            
                // Заменяем старое слово в списке
                bool alreadyAdded = false;
                for (int j = 0; j < newCount; j++) {
                    if (strcmp(newStaticTexts[j].text, word->text) == 0) {
                        alreadyAdded = true;
                        break;
                    }
                }
                if (!alreadyAdded) {
                    newStaticTexts[newCount++] = *word;
                }
                
                merged = true;
            } else {
                DEBUG_PRINT("Splitting word: '%s' at [%d, %d]\n", word->text, word->x, word->y);

                StaticText leftPart = CreateSubText(*word, word->x, actual_word.x - word->x);
                StaticText rightPart = CreateSubText(*word, actual_word.x + actual_word.width, (word->x + word->width) - (actual_word.x + actual_word.width));

                if (leftPart.text[0] != '\0') {
                    DEBUG_PRINT("Adding left part: '%s' at [%d, %d] width: %d\n", leftPart.text, leftPart.x, leftPart.y, leftPart.width);
                    newStaticTexts[newCount++] = leftPart;
                }

                if (rightPart.text[0] != '\0') {
                    DEBUG_PRINT("Adding right part: '%s' at [%d, %d] width: %d\n", rightPart.text, rightPart.x, rightPart.y, rightPart.width);
                    newStaticTexts[newCount++] = rightPart;
                }
            }
        }

        if (!merged) {
            DEBUG_PRINT("Adding new word: '%s' at [%d, %d] width: %d\n", actual_word.text, actual_word.x, actual_word.y, actual_word.width);
            newStaticTexts[newCount++] = actual_word;
        }
    }

    if (newCount > MAX_STATIC_TEXTS) {
        ERROR_PRINT("Too many static texts! Possible memory corruption, clearing line\n");
        ClearLine();
        return;
    }

    memcpy(staticTexts, newStaticTexts, newCount * sizeof(StaticText));
    staticTextCount = newCount;

    DEBUG_PRINT("Final staticTextCount: %d\n", staticTextCount);

    actual_word.x += actual_word.width;
    actual_word_len = 0;
    actual_word.width = 0;
}
