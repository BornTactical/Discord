# DiscordUpp
A simple discord library for creating C++ bots using Ultimate++

# Dependencies
* [Ultimate++](https://www.ultimatepp.org)
* No further dependencies are required

# Usage
Thus far DiscordUpp is not a full-featured Discord library, but it is enough to get started with a basic bot.

```cpp
#include <Discord/Discord.h>

using namespace Upp;

CONSOLE_APP_MAIN {
    Discord bot(<bot name>, <bot token>);
    
    bot.WhenMessageCreated = [&](ValueMap payload) {
        String channel  = payload["d"]["channel_id"];
        String content  = payload["d"]["content"];
        String userName = payload["d"]["username"];
        
        if(content.StartsWith("!hello")) {
            bot.CreateMessage(channel, "hello, world!");
        }
    };
    
    bot.Listen();
}
```
# A Minesweeper Bot 

This is a bot adapted from the Bombs (a Minesweeper clone) [example code](https://www.ultimatepp.org/examples$Bombs$en-us.html) on the Ultimate++ website. The build will require CtrlLib and the png plugin to operate since it renders to an image which is then posted into the channel.

```cpp
#include <Discord/Discord.h>
#include <CtrlLib/CtrlLib.h>
#include <plugin/png/png.h>

using namespace Upp;

class BombsBot {
    Discord bot {<name>, <token>};
    
    Size         level;
    int          cx, cy;
    int          normal_cells, bombs;
    Buffer<byte> field;
    bool         finished{false};
    byte& Field(int x, int y) { return field[x + y * cx]; }
    
    enum {
        HIDDEN = 16,
        BOMB = 32,
        MARK = 64,
        EXPLODED = 128,

        UNIT = 30,
    };
    
    void Refresh() {
        ImageDraw w(UNIT * cx - 1, UNIT * cy - 1);
        Paint(w);
        PNGEncoder png;
        Image m = w;
        png.SaveFile("temp.png", m);
    }
    
public:
    String GameInfo {
        "Bombs\nCommands: \n\t"
        "New game: new\n\t"
        "Uncover square: *u <x> <y>*\n\t"
        "Mark square: *m <x> <y>*"
    };
    
    String GetStatus() {
        if(finished)
            return "You have found all the bombs!";
        else
            return Format("%d bombs, %d cells remaining", bombs, normal_cells);
    }
    
    void Generate() {
        cx = level.cx;
        cy = level.cy;
        field.Alloc(cx * cy);
        for(int i = cx * cy - 1; i >= 0; i--)
            field[i] = (rand() & 15) < 3 ? HIDDEN|BOMB : HIDDEN;
        normal_cells = 0;
        for(int x = 0; x < cx; x++)
            for(int y = 0; y < cy; y++)
                if((Field(x, y) & BOMB) == 0) {
                    normal_cells++;
                    for(int xx = -1; xx <= 1; xx++)
                        for(int yy = -1; yy <= 1; yy++)
                            if((xx || yy) && x + xx >= 0 && x + xx < cx && y + yy >= 0 && y + yy < cy &&
                               (Field(x + xx, y + yy) & BOMB))
                                Field(x, y)++;
                }
        bombs = cx * cy - normal_cells;
        Refresh();
    }

    void UncoverAll() {
        for(int i = cx * cy - 1; i >= 0; i--)
            field[i] &= ~HIDDEN;
        Refresh();
    }

    void Paint(ImageDraw& w) {
        for(int x = 0; x < cx; x++)
            for(int y = 0; y < cy; y++) {
                byte f = Field(x, y);
                w.DrawRect(x * UNIT, y * UNIT + UNIT - 1, UNIT, 1, SBlack);
                w.DrawRect(x * UNIT + UNIT - 1, y * UNIT, 1, UNIT, SBlack);
                w.DrawRect(x * UNIT, y * UNIT, UNIT - 1, UNIT - 1,
                           (f & (HIDDEN|MARK)) ? SLtGray : f & BOMB ? SLtRed : SWhite);

                String txt;
                Color ink = SBlack;
                Color cross = Null;
                
                if(f & MARK) {
                    ink = SLtRed;
                    if((f & (HIDDEN|BOMB)) == BOMB) {
                        ink = SLtBlue;
                        cross = SLtRed;
                    }
                }
                else
                if(!(f & HIDDEN)) {
                    if(f & BOMB)
                        txt = "B";
                    else {
                        f = f & 15;
                        txt = String(f + '0', 1);
                        ink = f == 0 ? SLtGreen : f == 1 ? SLtBlue : SBlack;
                    }
                }
                Size tsz = GetTextSize(txt, Roman(2 * UNIT / 3));
                
                if(f & HIDDEN) {
                    txt = String(IntStr(y) + "," + IntStr(x), 3);
                    w.DrawText(x * UNIT + (UNIT - tsz.cx) / 2 - 13, y * UNIT + (UNIT - tsz.cy) / 2,
                           txt, Roman(2 * UNIT / 3), ink);
                    txt = "";
                }
                
                w.DrawText(x * UNIT + (UNIT - tsz.cx) / 2, y * UNIT + (UNIT - tsz.cy) / 2,
                           txt, Roman(2 * UNIT / 3), ink);
                if(f & EXPLODED)
                    cross = SLtBlue;
                w.DrawLine(x * UNIT, y * UNIT, x * UNIT + UNIT - 1, y * UNIT + UNIT - 1, 1, cross);
                w.DrawLine(x * UNIT, y * UNIT + UNIT - 1, x * UNIT + UNIT - 1, y * UNIT, 1, cross);
            }
    }

    void Mark(int x, int y) {
        if(Field(x, y) & HIDDEN) {
            Field(x, y) ^= MARK;
            Refresh();
        }
    }
    
    void Uncover(int x, int y) {
        if(x >= 0 && x < cx && y >= 0 && y < cy) {
            byte& f = Field(x, y);
            if((f & (HIDDEN|MARK)) == HIDDEN) {
                if(f & BOMB) {
                    f |= EXPLODED;
                    normal_cells = 0;
                    UncoverAll();
                    return;
                }
                if((f &= ~HIDDEN) == 0)
                    for(int xx = -1; xx <= 1; xx++)
                        for(int yy = -1; yy <= 1; yy++)
                            if(xx || yy)
                                Uncover(x + xx, y + yy);
                normal_cells--;
                if(normal_cells == 0) {
                    UncoverAll();
                    finished = true;
                }
            }
        }
        
        Refresh();
    }
    
    void Reset() {
        finished = false;
        level = Size(10, 10);
        Generate();
    }
    
    void Input(ValueMap payload) {
        String channel  = payload["d"]["channel_id"];
        String content  = payload["d"]["content"];
        String userName = payload["d"]["username"];
        
        if(content.StartsWith("!b")) {
            content.Remove(0, 3);
            
            try {
                CParser parser(content);
                String command = parser.ReadId();
                
                if(command == "new") {
                    Reset();
                    
                    bot.SendFile(channel, GameInfo, GetStatus(), "temp.png");
                }
                else if(command == "m") {
                    int x = parser.ReadInt();
                    int y = parser.ReadInt();
                    Mark(y, x);
                    
                    bot.SendFile(channel, GameInfo, GetStatus(), "temp.png");
                }
                else if(command == "u") {
                    int x = parser.ReadInt();
                    int y = parser.ReadInt();
                    Uncover(y, x);
                    
                    bot.SendFile(channel, GameInfo, GetStatus(), "temp.png");
                }
            }
            catch(CParser::Error exception) {
                bot.CreateMessage(channel, "Error: unknown command!");
            }
        }
    }
    
    void Run() {
        bot.Listen();
    }
    
    BombsBot() {
        bot.WhenMessageCreated << THISBACK(Input);
    }
    
    typedef BombsBot CLASSNAME;
};

CONSOLE_APP_MAIN {
    BombsBot bombs;
    bombs.Run();
}
```





