# DiscordUpp
A simple discord library for creating C++ bots under Ultimate++ (https://www.ultimatepp.org/)

# Dependencies
* Ultimate++ (https://www.ultimatepp.org)
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

