// Kaunsa page khula hai: "home", "dashboard" ya "admin"
const page = document.body.dataset.page;


// =====================================================
//  COMMON FUNCTIONS (teeno pages par kaam aate hain)
// =====================================================

// Backend ko request bhejta hai aur jawab laata hai
async function sendRequest(url, fields) {
    const data = new URLSearchParams();
    for (const key in fields) {
        data.append(key, fields[key]);
    }

    try {
        const response = await fetch(url, {
            method: "POST",
            headers: { "Content-Type": "application/x-www-form-urlencoded" },
            body: data.toString()
        });
        const result = await response.json();
        return { ok: response.ok, data: result };
    } catch (error) {
        alert("Cannot connect to the server. Please run bank.exe first.");
        return null;
    }
}

// Text ko HTML mein safe banata hai
function escapeHtml(text) {
    const div = document.createElement("div");
    div.textContent = text || "";
    return div.innerHTML;
}

// 1500 ko "Rs. 1500" banata hai
function formatMoney(amount) {
    return "Rs. " + Number(amount).toLocaleString("en-IN");
}

// Check karta hai ki amount sahi hai (poora number aur 0 se bada)
function validAmount(value) {
    const n = Number(value);
    if (value === "" || !Number.isInteger(n) || n <= 0) {
        alert("Please enter a valid amount (whole number greater than 0)");
        return false;
    }
    return true;
}

// Check karta hai ki PIN sahi 4 digit ka hai
function validPin(pin) {
    if (!/^\d{4}$/.test(pin)) {
        alert("PIN must be exactly 4 digits");
        return false;
    }
    return true;
}


// =====================================================
//  HOME PAGE (index.html): login aur create account
// =====================================================

// Login aur Create Account ke beech switch karta hai
function showTab(name) {
    document.getElementById("loginBox").classList.add("hidden");
    document.getElementById("createBox").classList.add("hidden");
    document.getElementById("loginTab").classList.remove("active");
    document.getElementById("createTab").classList.remove("active");

    if (name === "login") {
        document.getElementById("loginBox").classList.remove("hidden");
        document.getElementById("loginTab").classList.add("active");
    } else {
        document.getElementById("createBox").classList.remove("hidden");
        document.getElementById("createTab").classList.add("active");
    }
}

async function login() {
    const accNo = document.getElementById("loginAcc").value.trim();
    const pin = document.getElementById("loginPin").value.trim();

    if (accNo === "") {
        alert("Please enter your account number");
        return;
    }
    if (!validPin(pin)) return;

    const res = await sendRequest("/api/login", { accNo: accNo, pin: pin });
    if (res === null) return;

    if (res.ok) {
        // login ki jaankari browser mein yaad rakhte hain
        sessionStorage.setItem("accNo", accNo);
        sessionStorage.setItem("pin", pin);
        sessionStorage.setItem("name", res.data.name);
        window.location.href = "dashboard.html";
    } else {
        alert(res.data.error);
    }
}

async function createAccount() {
    const name = document.getElementById("newName").value.trim();
    const phone = document.getElementById("newPhone").value.trim();
    const pin = document.getElementById("newPin").value.trim();
    const deposit = document.getElementById("newDeposit").value.trim();

    if (name === "") {
        alert("Please enter your name");
        return;
    }
    if (!/^\d{10}$/.test(phone)) {
        alert("Phone number must be exactly 10 digits");
        return;
    }
    if (!validPin(pin)) return;
    if (!validAmount(deposit)) return;

    const res = await sendRequest("/api/create", {
        name: name, phone: phone, pin: pin, deposit: deposit
    });
    if (res === null) return;

    if (res.ok) {
        alert("Account created successfully!\nYour account number is: " + res.data.accNo +
              "\nPlease remember it.");
        showTab("login");
        document.getElementById("loginAcc").value = res.data.accNo;
    } else {
        alert(res.data.error);
    }
}


// =====================================================
//  DASHBOARD PAGE (dashboard.html): customer ka page
// =====================================================

// Login ki jaankari (account number aur PIN) har request ke saath bhejte hain
function me() {
    return {
        accNo: sessionStorage.getItem("accNo"),
        pin: sessionStorage.getItem("pin")
    };
}

// Page khulte hi chalta hai
async function startDashboard() {
    // login nahi kiya to wapas home page par bhejo
    if (sessionStorage.getItem("accNo") === null) {
        window.location.href = "index.html";
        return;
    }

    document.getElementById("welcome").textContent = "Welcome, " + sessionStorage.getItem("name");
    document.getElementById("accNoText").textContent = sessionStorage.getItem("accNo");
    await refreshBalance();
}

// Backend se naya balance laakar dikhata hai
async function refreshBalance() {
    const res = await sendRequest("/api/balance", me());
    if (res === null) return;

    if (res.ok) {
        document.getElementById("balanceText").textContent = formatMoney(res.data.balance);
    } else {
        // account band ho gaya ho ya PIN badal gaya ho
        alert(res.data.error);
        logout();
    }
}

// Ek panel dikhata hai, baaki chhupa deta hai
function showPanel(id) {
    const panels = ["depositPanel", "withdrawPanel", "transferPanel", "historyPanel", "pinPanel"];
    panels.forEach(function (p) {
        document.getElementById(p).classList.add("hidden");
    });
    document.getElementById(id).classList.remove("hidden");

    if (id === "historyPanel") {
        loadHistory();
    }
}

function logout() {
    sessionStorage.clear();
    window.location.href = "index.html";
}

// ---------- DEPOSIT ----------
async function deposit() {
    const amount = document.getElementById("depositAmount").value.trim();
    if (!validAmount(amount)) return;

    const res = await sendRequest("/api/deposit", Object.assign({}, me(), { amount: amount }));
    if (res === null) return;

    if (res.ok) {
        alert(res.data.message);
        document.getElementById("depositAmount").value = "";
        document.getElementById("balanceText").textContent = formatMoney(res.data.balance);
    } else {
        alert(res.data.error);
    }
}

// ---------- WITHDRAW ----------
async function withdraw() {
    const amount = document.getElementById("withdrawAmount").value.trim();
    if (!validAmount(amount)) return;

    const res = await sendRequest("/api/withdraw", Object.assign({}, me(), { amount: amount }));
    if (res === null) return;

    if (res.ok) {
        alert(res.data.message);
        document.getElementById("withdrawAmount").value = "";
        document.getElementById("balanceText").textContent = formatMoney(res.data.balance);
    } else {
        alert(res.data.error);
    }
}

// ---------- TRANSFER ----------
async function transfer() {
    const to = document.getElementById("transferTo").value.trim();
    const amount = document.getElementById("transferAmount").value.trim();

    if (to === "") {
        alert("Please enter the receiver's account number");
        return;
    }
    if (!validAmount(amount)) return;

    const res = await sendRequest("/api/transfer", Object.assign({}, me(), { to: to, amount: amount }));
    if (res === null) return;

    if (res.ok) {
        alert(res.data.message);
        document.getElementById("transferTo").value = "";
        document.getElementById("transferAmount").value = "";
        document.getElementById("balanceText").textContent = formatMoney(res.data.balance);
    } else {
        alert(res.data.error);
    }
}

// ---------- TRANSACTION HISTORY ----------
async function loadHistory() {
    const res = await sendRequest("/api/history", me());
    if (res === null) return;

    const body = document.getElementById("historyBody");
    body.innerHTML = "";

    if (!res.ok) {
        alert(res.data.error);
        return;
    }

    if (res.data.length === 0) {
        body.innerHTML = "<tr><td colspan='6'>No transactions yet.</td></tr>";
        return;
    }

    // nayi transaction sabse upar dikhe
    res.data.reverse();

    res.data.forEach(function (t) {
        // paisa aaya (credit) ya gaya (debit)
        const isCredit = (t.type === "Deposit" || t.type === "Transfer In");
        const sign = isCredit ? "+ " : "- ";
        const colorClass = isCredit ? "credit" : "debit";

        const row = document.createElement("tr");
        row.innerHTML =
            "<td>" + t.id + "</td>" +
            "<td>" + escapeHtml(t.date) + "</td>" +
            "<td>" + escapeHtml(t.type) + "</td>" +
            "<td class='" + colorClass + "'>" + sign + formatMoney(t.amount) + "</td>" +
            "<td>" + formatMoney(t.balance_after) + "</td>" +
            "<td>" + escapeHtml(t.note) + "</td>";
        body.appendChild(row);
    });
}

// ---------- CHANGE PIN ----------
async function changePin() {
    const oldPin = document.getElementById("oldPin").value.trim();
    const newPin = document.getElementById("newPinValue").value.trim();

    if (!validPin(oldPin)) return;
    if (!validPin(newPin)) return;

    // pin = purana PIN (login check ke liye), newpin = naya PIN
    const res = await sendRequest("/api/changepin", {
        accNo: sessionStorage.getItem("accNo"),
        pin: oldPin,
        newpin: newPin
    });
    if (res === null) return;

    if (res.ok) {
        alert(res.data.message);
        sessionStorage.setItem("pin", newPin);   // naya PIN yaad rakho
        document.getElementById("oldPin").value = "";
        document.getElementById("newPinValue").value = "";
    } else {
        alert(res.data.error);
    }
}


// =====================================================
//  ADMIN PAGE (admin.html)
// =====================================================

let allAccounts = [];   // backend se aaye saare accounts

async function adminLogin() {
    const password = document.getElementById("adminPassword").value;
    if (password === "") {
        alert("Please enter the admin password");
        return;
    }

    const res = await sendRequest("/api/admin/accounts", { password: password });
    if (res === null) return;

    if (res.ok) {
        sessionStorage.setItem("adminPassword", password);
        document.getElementById("adminLoginBox").classList.add("hidden");
        document.getElementById("adminPanel").classList.remove("hidden");
        allAccounts = res.data;
        showAccounts();
    } else {
        alert(res.data.error);
    }
}

function adminLogout() {
    sessionStorage.clear();
    window.location.reload();
}

// Backend se saare accounts dobara laata hai
async function loadAccounts() {
    const res = await sendRequest("/api/admin/accounts",
                                  { password: sessionStorage.getItem("adminPassword") });
    if (res !== null && res.ok) {
        allAccounts = res.data;
    }
    showAccounts();
}

// Search ke hisaab se accounts ki table banata hai
function showAccounts() {
    const search = document.getElementById("adminSearch").value.toLowerCase().trim();

    const result = allAccounts.filter(function (a) {
        if (search === "") return true;
        return String(a.accNo).includes(search) || a.name.toLowerCase().includes(search);
    });

    document.getElementById("accountCount").textContent = "Total accounts: " + result.length;

    const body = document.getElementById("accountsBody");
    body.innerHTML = "";

    if (result.length === 0) {
        body.innerHTML = "<tr><td colspan='5'>No accounts found.</td></tr>";
        return;
    }

    result.forEach(function (a) {
        const row = document.createElement("tr");
        row.innerHTML =
            "<td>" + a.accNo + "</td>" +
            "<td>" + escapeHtml(a.name) + "</td>" +
            "<td>" + escapeHtml(a.phone) + "</td>" +
            "<td>" + formatMoney(a.balance) + "</td>" +
            "<td><button class='red' onclick='closeAccount(" + a.accNo + ")'>Close Account</button></td>";
        body.appendChild(row);
    });
}

function clearAdminSearch() {
    document.getElementById("adminSearch").value = "";
    showAccounts();
}

async function closeAccount(accNo) {
    if (!confirm("Are you sure you want to close account " + accNo + "?")) {
        return;
    }

    const res = await sendRequest("/api/admin/delete", {
        password: sessionStorage.getItem("adminPassword"),
        accNo: accNo
    });
    if (res === null) return;

    alert(res.ok ? res.data.message : res.data.error);
    loadAccounts();
}


// =====================================================
//  PAGE KHULTE HI KYA CHALAANA HAI
// =====================================================
if (page === "dashboard") {
    startDashboard();
}