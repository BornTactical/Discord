#include "Discord.h"

namespace Upp {
    void Discord::ApplyRateLimits(HttpRequest& req) {
        int Limit     = StrInt(req.GetHeader("x-ratelimit-limit"));
        int Remaining = StrInt(req.GetHeader("x-ratelimit-remaining"));
        int WaitUntil = StrInt(req.GetHeader("x-ratelimit-reset"));
        
        Time wait = TimeFromUTC(WaitUntil);
        auto waitSeconds = GetUTCSeconds(wait) + GetLeapSeconds(wait) + GetTimeZone() * 60;
        
        Time date;
        ScanWwwTime(req.GetHeader("date"), date);
        auto serverSeconds = GetUTCSeconds(date);
        
        auto TimeDiff = waitSeconds - serverSeconds;
        
        if(Remaining == 0) {
            Thread::Sleep((int)(TimeDiff * 1000));
        }
    }
    
    void Discord::Resume() {
        Json json;
            json("token", token)
                ("session_id", sessionID)
                ("seq", lastSequenceNum);
            
        ws.SendTextMasked(json);
    }
    
    void Discord::ObtainGatewayAddress() {
        req.New();
        String response = req.Url(baseUrl + "/api/gateway/bot").Execute();
        req.Close();
            
        gatewayAddr = ParseJSON(response)["url"];
            
        // Discord doesn't actually respond to wss:// in the request header
        // so, we have a temporary workaround
        gatewayAddr.Replace("wss://", "https://");
    }
    
    void Discord::SendHeartbeat() {
        //LOG("Sending heartbeat");
        Json ack;
        
        ack("op", int(OP::HEARTBEAT))
           ("d",  lastSequenceNum);
        
        //LOG(ack);
        ws.SendTextMasked(ack);
    }
    
    void Discord::Identify() {
        Json json;
        json
        ("op", int(OP::IDENTIFY))
        ("d", Json("token", token)
                  ("properties",
                  Json("$os", "Windows")
                      ("$browser", "disco")
                      ("$device", "disco"))
                      ("compress", false)
                  ("large_threshold", 250)
                  //("shard", JsonArray() << 0 << 1)
                  ("presence",
                  Json("game",
                       Json("name", "D-TOX")
                           ("type", 0))
                           ("status", "online")
                           ("since", int(Null))
                           ("afk", false))
        );
        //LOG(json);
        ws.SendTextMasked(json);
    }
    
    void Discord::Reconnect(ValueMap payload) {
        keepRunning = false;
        ws.Close();
        Listen();
    }
    
    void Discord::Hello(ValueMap payload) {
        heartbeatInterval = (dword)(int)payload["d"]["heartbeat_interval"];
        sessionID         = payload["d"]["session_id"];
        Identify();
    }
    
    void Discord::Dispatch(ValueMap&& payload) {
        String dispatchEvent = payload["t"];
        lastSequenceNum      = payload["s"];
            
        if(dispatchEvent == "READY")
            WhenReady(pick(payload));
        else if(dispatchEvent == "ERROR")
            WhenError(pick(payload));
        else if(dispatchEvent == "GUILD_STATUS")
            WhenGuildStatusChanged(pick(payload));
        else if(dispatchEvent == "GUILD_CREATE")
            WhenGuildCreated(pick(payload));
        else if(dispatchEvent == "CHANNEL_CREATE")
            WhenChannelCreated(pick(payload));
        else if(dispatchEvent == "VOICE_CHANNEL_SELECT")
            WhenVoiceChannelSelected(pick(payload));
        else if(dispatchEvent == "VOICE_STATE_UPDATE")
            WhenVoiceStateUpdated(pick(payload));
        else if(dispatchEvent == "VOICE_STATE_DELETE")
            WhenVoiceStateDeleted(pick(payload));
        else if(dispatchEvent == "VOICE_SETTINGS_UPDATE")
            WhenVoiceSettingsUpdated(pick(payload));
        else if(dispatchEvent == "VOICE_CONNECTION_STATUS")
            WhenVoiceConnectionStatusChanged(pick(payload));
        else if(dispatchEvent == "SPEAKING_START")
            WhenSpeakingStarted(pick(payload));
        else if(dispatchEvent == "SPEAKING_STOP")
            WhenSpeakingStopped(pick(payload));
        else if(dispatchEvent == "MESSAGE_CREATE")
            WhenMessageCreated(pick(payload));
        else if(dispatchEvent == "MESSAGE_UPDATE")
            WhenMessageUpdated(pick(payload));
        else if(dispatchEvent == "MESSAGE_DELETE")
            WhenMessageDeleted(pick(payload));
        else if(dispatchEvent == "NOTIFICATION_CREATE")
            WhenNotificationCreated(pick(payload));
        else if(dispatchEvent == "CAPTURE_SHORTCUT_CHANGE")
            WhenCaptureShortcutChanged(pick(payload));
        else if(dispatchEvent == "ACTIVITY_JOIN")
            WhenActivityJoined(pick(payload));
        else if(dispatchEvent == "ACTIVITY_SPECTATE")
            WhenActivitySpectated(pick(payload));
        else if(dispatchEvent == "ACTIVITY_JOIN_REQUEST")
            WhenActivityJoinRequested(pick(payload));
        else if(dispatchEvent == "PRESENCES_REPLACE")
            WhenPresencesReplaced(pick(payload));
        else if(dispatchEvent == "SESSIONS_REPLACE")
            WhenSessionsReplaced(pick(payload));
        else if(dispatchEvent == "PRESENCE_UPDATE")
            WhenPresenceUpdated(pick(payload));
        else if(dispatchEvent == "USER_SETTINGS_UPDATE")
            WhenUserSettingsUpdated(pick(payload));
    }
    
    void Discord::InvalidSession(ValueMap payload) {
        bool resumable = payload["d"];
        if(!resumable) {
            //LOG("Invalid session, gateway unable to resume!");
            Exit();
        }
    }
    
    void Discord::GetGuildMembers(String guildId, int max, int after, ValueArray& payload) {
        String response =
            req.Url(baseUrl)
            .Path("/api/guilds/" + guildId + "/members?limit=" + AsString(max))
            .GET()
            .Execute();
            
        payload = ParseJSON(response);
        ApplyRateLimits(req);
    }
    
    void Discord::GetCurrentUser(ValueMap& payload) {
        String response =
            req.Url(baseUrl)
            .Path("/api/users/@me")
            .GET()
            .Execute();
        payload = ParseJSON(response);
        ApplyRateLimits(req);
    }
    
    void Discord::GetChannel(String channel, ValueMap& payload) {
        String response =
            req.Url(baseUrl)
               .Path("/api/channels/" + channel)
               .GET()
               .Execute();
        
        payload = ParseJSON(response);
        ApplyRateLimits(req);
    }
    
    void Discord::GetChannelMessage(String channel, String messageId, ValueMap& payload) {
        String response =
            req.Url(baseUrl)
               .Path("/api/channels/" + channel + "/messages/" + messageId)
               .GET()
               .Execute();
        
        payload = ParseJSON(response);
        ApplyRateLimits(req);
    }
    
    void Discord::GetMessages(String channel, int limit, ValueArray& payload) {
        String response =
            req.Url(baseUrl)
               .Path("/api/channels/" + channel + "/messages?limit=" + AsString(limit))
               .GET()
               .Execute();
    
        payload = ParseJSON(response);
        ApplyRateLimits(req);
    }
    
    void Discord::GetMessagesAfter(String channel, String messageId, int limit, ValueArray& payload) {
        String response =
            req.Url(baseUrl)
               .Path("/api/channels/" + channel + "/messages?after=" + messageId + "?limit=" + AsString(limit))
               .GET()
               .Execute();
    
        payload = ParseJSON(response);
        ApplyRateLimits(req);
    }

    void Discord::GetMessagesBefore(String channel, String messageId, int limit, ValueArray& payload) {
        String response =
            req.Url(baseUrl)
               .Path("/api/channels/" + channel + "/messages?before=" + messageId + "?limit=" + AsString(limit))
               .GET()
               .Execute();
    
        payload = ParseJSON(response);
        ApplyRateLimits(req);
    }

    void Discord::GetMessagesAround(String channel, String messageId, int limit, ValueArray& payload) {
        String response =
            req.Url(baseUrl)
               .Path("/api/channels/" + channel + "/messages?around=" + messageId + "?limit=" + AsString(limit))
               .GET()
               .Execute();
    
        payload = ParseJSON(response);
        ApplyRateLimits(req);
    }
    
    int Discord::GetMessageIds(String channel, int limit, JsonArray& messageIDs) {
        String response =
            req.Url(baseUrl)
               .Path("/api/channels/" + channel + "/messages?limit=" + AsString(limit))
               .GET()
               .Execute();
             
        ValueArray messages = ParseJSON(response);
        ApplyRateLimits(req);
        
        int i = 0;
        for(auto message : messages) {
            messageIDs << message["id"];
            i++;
        }
            
        return i;
    }
    
    void Discord::BulkDeleteMessages(String channel, int numToDelete) {
        req.New();
        
        int numDeleted = 0;
        
        do {
            JsonArray messageIDs;
            numDeleted = GetMessageIds(channel, numToDelete > 100 ? 100 : numToDelete, messageIDs);
            
            Json json("messages", messageIDs);
            numToDelete -= 100;
            String response =
                req.Url(baseUrl)
                    .Path("/api/channels/" + channel + "/messages/bulk-delete")
                    .POST()
                    .Post(json)
                    .Execute();
               
            ValueMap m = ParseJSON(response);
            ApplyRateLimits(req);
        }
        while(numDeleted == 100);
    }
    
    void Discord::CreateMessage(String channel, String message) {
        req.New();
        Json json("content", message);
        
        String response =
            req.Url(baseUrl)
               .Path("/api/channels/" + channel + "/messages")
               .POST()
               .Post(json)
               .Execute();
      
        ValueMap m = ParseJSON(response);
        ApplyRateLimits(req);
    }
    
    void Discord::NotifyStartTyping(String channel) {
        req.New();
        req.Url(baseUrl)
           .Path("/api/channels/" + channel + "/typing")
           .POST()
           .Execute();
        ApplyRateLimits(req);
    }
    
    void Discord::SendFile(String channel, String message, String title, String fileName) {
        req.New();
        String file = LoadFile(fileName);
        
        Json json;
        json
            ("content", message)
            ("embed",
            Json("title", title)
                ("image",
                Json("url", "attachment://" + fileName)));
                    
        req.ClearPost();
        req.ClearContent();
        req.ContentType("multipart/form-data");
        req.Part("file", file, FileExtToMIME(fileName), fileName)
           .Part("payload_json", json, "application/json");
    
        String response =
            req.Url(baseUrl)
               .Path("/api/channels/" + channel + "/messages")
               .POST()
               .Execute();
               
        ValueMap m = ParseJSON(response);
        ApplyRateLimits(req);
        
        req.ClearPost();
        req.ClearContent();
        req.ContentType("application/json");
    }
    
    void Discord::EventLoop() {
        auto now = GetTickCount();
        dword last = 0;
        
        // Event loop
        while(!done) {
            now = GetTickCount();
            BeforeSocketReceive();
            String response = ws.Receive();
            
            if(ws.IsError()) {
                //LOG(ws.GetError());
                return;
            }
            else if(ws.IsClosed()) {
                //LOG("Socket closed unexpectedly");
                //LOG(ws.GetError());
                return;
            }
            
            if(response.IsEmpty()) {
                if((now - last) > heartbeatInterval) {
                    last = now;
                    SendHeartbeat();
                }
                
                Thread::Sleep(100);
                continue;
            }
            
            ValueMap payload = ParseJSON(response);
            int op = payload["op"];
            
            switch(op) {
                case OP::DISPATCH:
                    Dispatch(pick(payload));
                    break;
                case OP::HEARTBEAT:
                    break;
                case OP::RECONNECT:
                    Reconnect(payload);
                    break;
                case OP::INVALID_SESSION:
                    InvalidSession(payload);
                    break;
                case OP::HELLO:
                    Hello(payload);
                    break;
                case OP::HEARTBEAT_ACK:
                    HeartbeatAck(payload);
                    break;
                default:
                    break;
            }
        }
    }
    
    void Discord::ListenDetach() {
        eventThread.Run([this]{Listen();});
    }
    
    void Discord::Listen() {
        ws.NonBlocking(false);
        ws.Connect(gatewayAddr, "gateway.discord.gg", true, 443);
        ws.NonBlocking();
        
        // A reconnect event was received, so resuming the session to play back
        // whatever events were sent when the client was down
        if(shouldResume) {
            Resume();
            shouldResume = false;
        }
        
        EventLoop();
    }
    
    Discord::Discord() {
    }
    
    bool Discord::LoginAsUser(String userAgent, String userToken) {
        if(userToken.IsEmpty()) {
            return false;
        }
        
        token = userToken;
        
        req.ClearContent();
        req.Clear();
        req.New();
        req.New();
        
        req.Header("User-Agent", userAgent)
           .Header("Authorization", token)
           .ContentType("application/json");
        
        ws.Header("User-Agent", userAgent)
          .Header("Authorization", token)
          .Header("ContentType", "application/json");
        
        ObtainGatewayAddress();
        
        return true;
    }
    
    bool Discord::LoginAsUser(String userAgent, String uname, String password) {
        req.New();
        req.ContentType("application/json");
        Json json;
        json("email", uname)
            ("password", password);
        String response = req.Url(baseUrl + "/api/auth/login")
                             .Post(json)
                             .Execute();
                             
        Value payload = ParseJSON(response);
        token = payload["token"];
        
        if(token.IsEmpty()) {
            return false;
        }
        
        req.ClearContent();
        req.Clear();
        req.New();
        
        req.Header("User-Agent", userAgent)
           .Header("Authorization", token)
           .ContentType("application/json");
        
        ws.Header("User-Agent", userAgent)
          .Header("Authorization", token)
          .Header("ContentType", "application/json");
        
        ObtainGatewayAddress();
        
        return true;
    }
    
    void Discord::LoginAsBot(String name, String token) {
        this->token = token;
        this->name  = name;
        
        req.Header("User-Agent", name)
           .Header("Authorization", "Bot " + token)
           .ContentType("application/json");
           
        ws.Header("User-Agent", name)
          .Header("Authorization", "Bot " + token)
          .Header("ContentType", "application/json");
          
        ObtainGatewayAddress();
    }
    
    Discord::Discord(String configFile) {
        auto file = ParseJSON(LoadFile(configFile));
        name  = file["bot"]["name"];
        token = file["bot"]["token"];
        
        Discord(name, token);
    }
    
    Discord::Discord(String name, String token) : token(token), name(name) {
        req.Header("User-Agent", name)
           .Header("Authorization", "Bot " + token)
           .ContentType("application/json");
           
        ws.Header("User-Agent", name)
          .Header("Authorization", "Bot " + token)
          .Header("ContentType", "application/json");
          
        ObtainGatewayAddress();
    }
    
    Discord::~Discord() {
        done = true;
        eventThread.Wait();
        
        ws.Close("Closing websocket");
        req.Close();
    }
}













