#include "Discord.h"

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
		Thread::Sleep(TimeDiff * 1000);
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

void Discord::RepeatSendHeartbeat(unsigned int ms, std::atomic<bool>& keepRunning) {
	// This will execute on a different thread
	while(keepRunning) {
		Thread::Sleep(ms);
		LOG("Sending heartbeat");
		Json ack;

		ack("op", int(OP::HEARTBEAT))
		   ("d",  lastSequenceNum);
		
		LOG(ack);
		ws.SendTextMasked(ack);
	}
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
				   Json("name", "N/A")
					   ("type", 0))
					   ("status", "online")
					   ("since", int(Null))
					   ("afk", false))
	);
	LOG(json);
	ws.SendTextMasked(json);
}

void Discord::Reconnect(ValueMap payload) {
	keepRunning = false;
	ws.Close();
	Listen();
}

void Discord::Hello(ValueMap payload) {
	heartbeatInterval = payload["d"]["heartbeat_interval"];
	sessionID         = payload["d"]["session_id"];
	
	Thread().Run(THISBACK2(RepeatSendHeartbeat, heartbeatInterval, std::ref(keepRunning)));
	Identify();
}

void Discord::Dispatch(ValueMap payload) {
	String dispatchEvent = payload["t"];
	lastSequenceNum      = payload["s"];
		
	if(dispatchEvent == "READY")
		WhenReady(payload);
	else if(dispatchEvent == "ERROR")
		WhenError(payload);
	else if(dispatchEvent == "GUILD_STATUS")
		WhenGuildStatusChanged(payload);
	else if(dispatchEvent == "GUILD_CREATE")
		WhenGuildCreated(payload);
	else if(dispatchEvent == "CHANNEL_CREATE")
		WhenChannelCreated(payload);
	else if(dispatchEvent == "VOICE_CHANNEL_SELECT")
		WhenVoiceChannelSelected(payload);
	else if(dispatchEvent == "VOICE_STATE_UPDATE")
		WhenVoiceStateUpdated(payload);
	else if(dispatchEvent == "VOICE_STATE_DELETE")
		WhenVoiceStateDeleted(payload);
	else if(dispatchEvent == "VOICE_SETTINGS_UPDATE")
		WhenVoiceSettingsUpdated(payload);
	else if(dispatchEvent == "VOICE_CONNECTION_STATUS")
		WhenVoiceConnectionStatusChanged(payload);
	else if(dispatchEvent == "SPEAKING_START")
		WhenSpeakingStarted(payload);
	else if(dispatchEvent == "SPEAKING_STOP")
		WhenSpeakingStopped(payload);
	else if(dispatchEvent == "MESSAGE_CREATE")
		WhenMessageCreated(payload);
	else if(dispatchEvent == "MESSAGE_UPDATE")
		WhenMessageUpdated(payload);
	else if(dispatchEvent == "MESSAGE_DELETE")
		WhenMessageDeleted(payload);
	else if(dispatchEvent == "NOTIFICATION_CREATE")
		WhenNotificationCreated(payload);
	else if(dispatchEvent == "CAPTURE_SHORTCUT_CHANGE")
		WhenCaptureShortcutChanged(payload);
	else if(dispatchEvent == "ACTIVITY_JOIN")
		WhenActivityJoined(payload);
	else if(dispatchEvent == "ACTIVITY_SPECTATE")
		WhenActivitySpectated(payload);
	else if(dispatchEvent == "ACTIVITY_JOIN_REQUEST")
		WhenActivityJoinRequested(payload);
}

void Discord::InvalidSession(ValueMap payload) {
	bool resumable = payload["d"];
	if(!resumable) {
		LOG("Invalid session, gateway unable to resume!");
		Exit();
	}
}

int Discord::GetMessages(String channel, int limit, JsonArray& messageIDs) {
	String response =
		req.Url(baseUrl)
	       .Path("/api/channels/" + channel + "/messages?limit=" + AsString(limit))
	       .GET()
	       .Execute();
	       
	LOG(response);

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
		numDeleted = GetMessages(channel, numToDelete > 100 ? 100 : numToDelete, messageIDs);
		
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
	
	LOG(response);
	ValueMap m = ParseJSON(response);
	ApplyRateLimits(req);
	LOG(req.GetContent());
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
			Json("url", "attachment://" + fileName)
			    ("width",  320)
			    ("height", 200)));
			    
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
	       
	LOG(response);
	ValueMap m = ParseJSON(response);
	ApplyRateLimits(req);
	
	req.ClearPost();
	req.ClearContent();
	req.ContentType("application/json");
}


void Discord::Listen() {
	ws.Connect(gatewayAddr, "gateway.discord.gg", true, 443);
		
	// A reconnect event was received, so resuming the session to play back
	// whatever events were sent when the client was down
	if(shouldResume) {
		Resume();
		shouldResume = false;
	}
		
	// Event loop
	for(;;) {
		BeforeSocketReceive();
		String response = ws.Receive();
		
		if(ws.IsError()) {
			LOG(ws.GetError());
			return;
		}
		else if(ws.IsClosed()) {
			LOG("Socket closed unexpectedly");
			LOG(ws.GetError());
			return;
		}
		
		ValueMap payload = ParseJSON(response);
		int op = payload["op"];
		
		switch(op) {
			case OP::DISPATCH:
				Dispatch(payload);
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
	keepRunning = false;
	ws.Close("Closing websocket");
	req.Close();
}















