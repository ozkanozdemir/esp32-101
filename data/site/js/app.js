// Sohbet odası istemcisi. Kartla WebSocket (ws://<adres>/ws) üzerinden JSON mesajlaşır.
// Mesaj biçimleri için src/chat.h dosyasının başındaki açıklamaya bakın.

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
      updateEmpty();
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

    case "led":
      addLed(msg.from, msg.color);
      setLedSwatch(msg.color);
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
  $("login-dot").classList.toggle("online", online);
  $("login-status").textContent = online ? "Karta bağlandı" : "Karta bağlanıyor…";
  $("login-button").disabled = !online;
}

// ---------- İsim rozetleri (avatar) ----------

// Her isme sabit bir renk tonu: aynı isim herkeste aynı renkte görünür
function nameHue(name) {
  let hash = 0;
  for (const ch of name) hash = (hash * 31 + ch.codePointAt(0)) >>> 0;
  return hash % 360;
}

function makeAvatar(name) {
  const el = document.createElement("span");
  el.className = "avatar";
  el.textContent = Array.from(name)[0] || "?";
  el.setAttribute("aria-hidden", "true");
  return el;
}

function renderUsers(names) {
  $("user-count").textContent = names.length;
  const items = names
    .slice()
    .sort((a, b) => a.localeCompare(b, "tr"))
    .map((name) => {
      const li = document.createElement("li");
      li.style.setProperty("--hue", nameHue(name));
      if (name === myName) li.classList.add("me");
      const label = document.createElement("span");
      label.className = "name";
      label.textContent = name;
      li.append(makeAvatar(name), label);
      return li;
    });
  $("user-list").replaceChildren(...items);
}

// ---------- Mesaj listesi ----------

function formatTime(date) {
  return date.toLocaleTimeString("tr-TR", { hour: "2-digit", minute: "2-digit" });
}

function updateEmpty() {
  $("empty").hidden = $("messages").children.length > 0;
}

// Yeni mesaj gelince, kullanıcı zaten en alttaysa aşağı kaydır (yukarıda eski mesajları okuyorsa rahatsız etme)
function appendMessage(li) {
  const list = $("messages");
  const atBottom = list.scrollHeight - list.scrollTop - list.clientHeight < 80;
  list.append(li);
  if (atBottom) list.scrollTop = list.scrollHeight;
  updateEmpty();
}

// Not: Metinler her zaman textContent ile yazılır; böylece birinin mesajına yazdığı HTML/JS çalışmaz.
function addChat(from, text, date) {
  const li = document.createElement("li");
  li.className = from === myName ? "msg mine" : "msg";
  li.style.setProperty("--hue", nameHue(from));

  const bubble = document.createElement("div");
  bubble.className = "bubble";

  const fromEl = document.createElement("span");
  fromEl.className = "from";
  fromEl.textContent = from;

  const textEl = document.createElement("span");
  textEl.className = "text";
  textEl.textContent = text;

  const timeEl = document.createElement("time");
  timeEl.className = "time";
  timeEl.textContent = formatTime(date);

  bubble.append(fromEl, textEl, timeEl);
  li.append(makeAvatar(from), bubble);
  appendMessage(li);
}

function addSystem(text, isError = false) {
  const li = document.createElement("li");
  li.className = isError ? "system error" : "system";
  li.textContent = text;
  appendMessage(li);
}

function addLed(from, color) {
  const li = document.createElement("li");
  li.className = "system";
  const chip = document.createElement("span");
  chip.className = "chip";
  chip.style.background = color;
  // "Ali LED'i ● rengine çevirdi" / kendimiz için "LED'i ● rengine çevirdiniz"
  if (from === myName) li.append("LED'i", chip, "rengine çevirdiniz");
  else li.append(`${from} LED'i`, chip, "rengine çevirdi");
  appendMessage(li);
}

// ---------- LED renk seçici ----------

function setLedSwatch(color) {
  $("led-input").value = color;
  document.querySelector(".led-picker").style.setProperty("--led", color);
}

// Seçici açıkken renk değiştikçe sadece düğmedeki önizlemeyi güncelle
$("led-input").addEventListener("input", (event) => {
  document.querySelector(".led-picker").style.setProperty("--led", event.target.value);
});

// Seçim bitince (seçici kapanınca) rengi karta gönder
$("led-input").addEventListener("change", (event) => {
  send({ type: "led", color: event.target.value });
});

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
