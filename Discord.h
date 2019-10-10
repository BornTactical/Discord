#ifndef _Discord_Discord_h_
#define _Discord_Discord_h_

#include <Core/Core.h>
#include <atomic>

namespace Upp {
    enum Permissions {
        CREATE_INSTANT_INVITE = 0x00000001,
        KICK_MEMBERS          = 0x00000002,
        BAN_MEMBERS           = 0x00000004,
        ADMINISTRATOR         = 0x00000008,
        MANAGE_CHANNELS       = 0x00000010,
        MANAGE_GUILD          = 0x00000020,
        ADD_REACTIONS         = 0x00000040,
        VIEW_AUDIT_LOG        = 0x00000080,
        VIEW_CHANNEL          = 0x00000400,
        SEND_MESSAGES         = 0x00000800,
        SEND_TTS_MESSAGES     = 0x00001000,
        MANAGE_MESSAGES       = 0x00002000,
        EMBED_LINKS           = 0x00004000,
        ATTACH_FILES          = 0x00008000,
        READ_MESSAGE_HISTORY  = 0x00010000,
        MENTION_EVERYONE      = 0x00020000,
        USE_EXTERNAL_EMOJIS   = 0x00040000,
        CONNECT               = 0x00100000,
        SPEAK                 = 0x00200000,
        MUTE_MEMBERS          = 0x00400000,
        DEAFEN_MEMBERS        = 0x00800000,
        MOVE_MEMBERS          = 0x01000000,
        USE_VAD               = 0x02000000,
        PRIORITY_SPEAKER      = 0x00000100,
        CHANGE_NICKNAME       = 0x04000000,
        MANAGE_NICKNAMES      = 0x08000000,
        MANAGE_ROLES          = 0x10000000,
        MANAGE_WEBHOOKS       = 0x20000000,
        MANAGE_EMOJIS         = 0x40000000,
        ALL                   =
        (CREATE_INSTANT_INVITE | KICK_MEMBERS     | BAN_MEMBERS          | ADMINISTRATOR    |
         MANAGE_CHANNELS       | MANAGE_GUILD     | ADD_REACTIONS        | VIEW_AUDIT_LOG   |
         VIEW_CHANNEL          | SEND_MESSAGES    | SEND_TTS_MESSAGES    | MANAGE_MESSAGES  |
         EMBED_LINKS           | ATTACH_FILES     | READ_MESSAGE_HISTORY | MENTION_EVERYONE |
         USE_EXTERNAL_EMOJIS   | CONNECT          | SPEAK                | MUTE_MEMBERS     |
         DEAFEN_MEMBERS        | MOVE_MEMBERS     | USE_VAD              | PRIORITY_SPEAKER |
         CHANGE_NICKNAME       | MANAGE_NICKNAMES | MANAGE_ROLES         | MANAGE_WEBHOOKS  |
         MANAGE_EMOJIS),
        NONE                  = 0x0
    };
        
    enum OP {
        DISPATCH,
        HEARTBEAT,
        IDENTIFY,
        STATUS_UPDATE,
        VOICE_STATUS_UPDATE,
        VOICE_SERVER_PING,
        RESUME,
        RECONNECT,
        REQUEST_GUILD_MEMBERS,
        INVALID_SESSION,
        HELLO,
        HEARTBEAT_ACK
    };
    
    struct Discord {
        Thread eventThread;
        Thread heartbeatThread;
        
        String gatewayAddr = "https://gateway.discord.gg";
        String baseUrl     = "https://discordapp.com";
        String token                  { Null  };
        String name                   { Null  };
        Value  sessionID              { Null  };
        Value  lastSequenceNum        { Null  };
        dword  heartbeatInterval      { 10000 };
        std::atomic<bool> keepRunning { true  };
        std::atomic<bool> done        { false };
        bool   shouldResume           { false };
        
        // rate limiting
        void ApplyRateLimits(HttpRequest& req);
            
        WebSocket   ws;
        HttpRequest req;
        
        void Resume();
        void ObtainGatewayAddress();
        void SendHeartbeat();
        void Identify();
        void Reconnect(ValueMap payload);
        void Hello(ValueMap payload);
        void HeartbeatAck(ValueMap payload) { LOG(payload); }
        void Dispatch(ValueMap&& payload);
        void InvalidSession(ValueMap payload);
        
    public:
        Event<const ValueMap> WhenReady,
            WhenError,
            WhenGuildStatusChanged,
            WhenGuildCreated,
            WhenChannelCreated,
            WhenVoiceChannelCreated,
            WhenVoiceChannelSelected,
            WhenVoiceStateUpdated,
            WhenVoiceStateDeleted,
            WhenVoiceSettingsUpdated,
            WhenVoiceConnectionStatusChanged,
            WhenSpeakingStarted,
            WhenSpeakingStopped,
            WhenMessageCreated,
            WhenMessageUpdated,
            WhenMessageDeleted,
            WhenNotificationCreated,
            WhenCaptureShortcutChanged,
            WhenActivityJoined,
            WhenActivitySpectated,
            WhenActivityJoinRequested,
            WhenPresencesReplaced,
            WhenPresenceUpdated,
            WhenSessionsReplaced,
            WhenUserSettingsUpdated;
        
        Event<> BeforeSocketReceive;
        
        static void GetRole(const Value& guild, String roleId, Value& outRole) {
            for(auto role : guild["roles"]) {
                if(role["id"] == roleId) {
                    outRole = role;
                }
            }
        }
        
        static void GetOverwrite(const Value& channel, String overwriteId, Value& outOverwrite) {
            for(auto overwrite : channel["permission_overwrites"]) {
                if(overwrite["id"] == overwriteId) {
                    outOverwrite = overwrite;
                }
            }
        }

        static int ComputeBasePermissions(const Value& guild, const Value& member) {
            int permissions = 0;
            
            if(guild["owner_id"] == member["id"]) {
                return ALL;
            }
            
            Value everyone = Null;
            GetRole(guild, guild["id"], everyone);
            
            permissions = everyone["permissions"];
            
            for(auto role : member["roles"]) {
                permissions |= (int)role["permissions"];
            }
            
            if((permissions & ADMINISTRATOR) == ADMINISTRATOR) return ALL;

            return permissions;
        }
        
        static int ComputeOverwrites(int basePermissions, const Value& guild, const Value& member, const Value& channel) {
            if((basePermissions & ADMINISTRATOR) == ADMINISTRATOR) return ALL;
            
            Value overwriteEveryone = Null;
            
            int permissions = basePermissions;
            GetOverwrite(channel, guild["id"], overwriteEveryone);
            
            if(!overwriteEveryone.IsNull()) {
                permissions &= ~(int)overwriteEveryone["deny"];
                permissions |= (int)overwriteEveryone["allow"];
            }
            
            auto overwrites = channel["overwrites"];
            int allow = NONE;
            int deny  = NONE;
            
            for(auto roleId : member["roles"]["id"]) {
                Value overwriteRole = Null;
                GetOverwrite(channel, roleId, overwriteRole);
                
                if(!overwriteRole.IsNull()) {
                    allow |= (int)overwriteRole["allow"];
                    deny  |= (int)overwriteRole["deny"];
                }
            }
            
            permissions &= ~deny;
            permissions |= allow;
            
            Value overwriteMember = Null;
            GetOverwrite(channel, member["id"], overwriteMember);
            
            if(!overwriteMember.IsNull()) {
                permissions &= ~(int)overwriteMember["deny"];
                permissions |= (int)overwriteMember["allow"];
            }
            
            return permissions;
        }
        
        static int ComputePermissions(const Value& guild, const Value& member, const Value& channel) {
            auto basePermissions = ComputeBasePermissions(guild, member);
            return ComputeOverwrites(basePermissions, guild, member, channel);
        }

        void GetGuildMembers(String guildId, int max, int start, ValueArray& payload);
        void GetChannel(String channel, ValueMap& payload);
        void GetChannelMessage(String channel, String messageId, ValueMap& payload);
        void GetCurrentUser(ValueMap& payload);
        
        void GetMessages(String channel, int count, ValueArray& payload);
        void GetMessagesAfter(String channel, String messageId, int count, ValueArray& payload);
        void GetMessagesBefore(String channel, String messageId, int count, ValueArray& payload);
        void GetMessagesAround(String channel, String messageId, int count, ValueArray& payload);
        
        int  GetMessageIds(String channel, int count, JsonArray& messageIDs);
        void SendFile(String channel, String message, String title, String fileName);
        void BulkDeleteMessages(String channel, int count);
        void CreateMessage(String channel, String message);
        void Listen();
        void EventLoop();
        void ListenDetach();
        void NotifyStartTyping(String channel);
        
        bool LoginAsUser(String userAgent, String token);
        bool LoginAsUser(String userAgent, String uname, String password);
        void LoginAsBot(String name, String token);
        
        Discord();
        Discord(String configFile);
        Discord(String name, String token);
        ~Discord();
        
        typedef Discord CLASSNAME;
    };
}
#endif
