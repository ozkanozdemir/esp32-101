// Sohbet odası istemcisi. Kartla WebSocket (ws://<adres>/ws) üzerinden JSON mesajlaşır.
// Mesaj biçimleri için src/main.cpp dosyasının başındaki açıklamaya bakın.

const $ = (id) => document.getElementById(id);

const NAME_KEY = "chat.name";   // localStorage: son kullanılan isim (form doldurmak için)
// sessionStorage (sekmeye özel): sayfa yenilenince aynı isimle odaya geri dönmek için
const SESSION_NAME_KEY = "chat.sessionName";
const TOKEN_KEY = "chat.token";
const RECONNECT_MS = 2000;

let socket = null;
let myName = null;      // odaya katılınca dolar
let wantJoin = false;   // kullanıcı odada olmak istiyor mu (kopunca yeniden katılmak için)
let reconnectTimer = null;

// ---------- Tarayıcı depolaması (gizli sekmede hata verebilir) ----------

function load(storage, key) {
  try { return storage.getItem(key); } catch { return null; }
}

function save(storage, key, value) {
  try {
    if (value === null) storage.removeItem(key);
    else storage.setItem(key, value);
  } catch { /* depolama kapalıysa sessizce geç */ }
}

// ---------- WebSocket bağlantısı ----------

function connect() {
  if (socket && socket.readyState <= WebSocket.OPEN) return;
  clearTimeout(reconnectTimer);
  setOnline(false);

  socket = new WebSocket(`ws://${location.host}/ws`);

  socket.addEventListener("open", () => {
    setOnline(true);
    if (wantJoin) sendJoin();
  });

  socket.addEventListener("message", (event) => {
    let msg;
    try { msg = JSON.parse(event.data); } catch { return; }
    handleMessage(msg);
  });

  socket.addEventListener("close", () => {
    setOnline(false);
    if (myName) addSystem("Bağlantı koptu, yeniden bağlanılıyor…");
    myName = null;
    // Kullanıcı odada kalmak istiyorsa ya da giriş ekranındaysa tekrar bağlan
    reconnectTimer = setTimeout(connect, RECONNECT_MS);
  });
}

function send(obj) {
  if (socket && socket.readyState === WebSocket.OPEN) {
    socket.send(JSON.stringify(obj));
    return true;
  }
  return false;
}

function sendJoin() {
  send({ type: "join", name: $("name-input").value.trim(), token: load(sessionStorage, TOKEN_KEY) || "" });
}

// ---------- Karttan gelen mesajlar ----------

function handleMessage(msg) {
  switch (msg.type) {
    case "welcome":
      myName = msg.name;
      save(sessionStorage, TOKEN_KEY, msg.token);
      save(sessionStorage, SESSION_NAME_KEY, msg.name);
      save(localStorage, NAME_KEY, msg.name);
      showChat();
      break;

    case "history":
      $("messages").replaceChildren();
      for (const m of msg.messages) {
        addChat(m.from, m.text, new Date(Date.now() - m.ago * 1000));
      }
      break;

    case "users":
      renderUsers(msg.users);
      break;

    case "chat":
      addChat(msg.from, msg.text, new Date());
      break;

    case "system":
      addSystem(msg.text);
      break;

    case "error":
      if (myName) {
        addSystem(msg.text, true);
      } else {
        // Giriş reddedildi (isim alınmış vb.): giriş ekranında göster, otomatik deneme yapma
        wantJoin = false;
        save(sessionStorage, TOKEN_KEY, null);
        save(sessionStorage, SESSION_NAME_KEY, null);
        showLogin(msg.text);
      }
      break;
  }
}

// ---------- Ekranlar ----------

function showLogin(error) {
  $("chat").hidden = true;
  $("login").hidden = false;
  $("login-error").textContent = error || "";
  $("login-error").hidden = !error;
  $("name-input").focus();
}

function showChat() {
  $("login").hidden = true;
  $("chat").hidden = false;
  $("message-input").focus();
}

function setOnline(online) {
  $("status-dot").classList.toggle("online", online);
  $("login-status").textContent = online ? "Bağlandı" : "Bağlanıyor…";
  $("login-button").disabled = !online;
}

function renderUsers(names) {
  $("user-count").textContent = names.length;
  const items = names
    .slice()
    .sort((a, b) => a.localeCompare(b, "tr"))
    .map((name) => {
      const li = document.createElement("li");
      li.textContent = name === myName ? `${name} (sen)` : name;
      if (name === myName) li.classList.add("me");
      return li;
    });
  $("user-list").replaceChildren(...items);
}

function formatTime(date) {
  return date.toLocaleTimeString("tr-TR", { hour: "2-digit", minute: "2-digit" });
}

// Yeni mesaj gelince, kullanıcı zaten en alttaysa aşağı kaydır (yukarıda eski mesajları okuyorsa rahatsız etme)
function appendMessage(li) {
  const list = $("messages");
  const atBottom = list.scrollHeight - list.scrollTop - list.clientHeight < 60;
  list.append(li);
  if (atBottom) list.scrollTop = list.scrollHeight;
}

// Not: Metinler her zaman textContent ile yazılır; böylece birinin mesajına yazdığı HTML/JS çalışmaz.
function addChat(from, text, date) {
  const li = document.createElement("li");
  li.className = from === myName ? "msg mine" : "msg";

  const fromEl = document.createElement("span");
  fromEl.className = "from";
  fromEl.textContent = from;

  const textEl = document.createElement("span");
  textEl.className = "text";
  textEl.textContent = text;

  const timeEl = document.createElement("time");
  timeEl.className = "time";
  timeEl.textContent = formatTime(date);

  li.append(fromEl, textEl, timeEl);
  appendMessage(li);
}

function addSystem(text, isError = false) {
  const li = document.createElement("li");
  li.className = isError ? "system error" : "system";
  li.textContent = text;
  appendMessage(li);
}

// ---------- Kullanıcı etkileşimleri ----------

$("login-form").addEventListener("submit", (event) => {
  event.preventDefault();
  const name = $("name-input").value.trim();
  if (!name) return;
  wantJoin = true;
  $("login-error").hidden = true;
  if (!send({ type: "join", name, token: load(sessionStorage, TOKEN_KEY) || "" })) connect();
});

$("message-form").addEventListener("submit", (event) => {
  event.preventDefault();
  const input = $("message-input");
  const text = input.value.trim();
  if (!text) return;
  if (send({ type: "chat", text })) input.value = "";
});

$("leave-button").addEventListener("click", () => {
  wantJoin = false;
  myName = null;
  save(sessionStorage, TOKEN_KEY, null);
  save(sessionStorage, SESSION_NAME_KEY, null);
  // Karta çıktığımızı söyle, sonra bağlantıyı kapat;
  // close olayı giriş ekranı için yeni bir bağlantı açar
  send({ type: "leave" });
  if (socket) socket.close();
  showLogin();
});

$("users-toggle").addEventListener("click", () => {
  const open = $("users").classList.toggle("open");
  $("users-toggle").setAttribute("aria-expanded", open);
});

// ---------- Başlangıç ----------

// Sayfa yenilendiyse (bu sekmede isim ve anahtar varsa) kullanıcıyı aynı isimle odaya geri al
const sessionName = load(sessionStorage, SESSION_NAME_KEY);
wantJoin = Boolean(sessionName && load(sessionStorage, TOKEN_KEY));
$("name-input").value = wantJoin ? sessionName : load(localStorage, NAME_KEY) || "";
connect();
