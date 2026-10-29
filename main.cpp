#include <iostream>
#include <ctime>
#include <cctype>
#include "httplib.h"
#include "../database/database.h"
using namespace std;
using namespace httplib;

// ---------- BANK KE RULES ----------
const long long MIN_BALANCE = 500;         // account mein kam se kam itna paisa bacha rahega
const long long MAX_AMOUNT = 10000000;     // ek baar mein maximum Rs. 1 crore
const string ADMIN_PASSWORD = "admin123";  // admin ka password

// ---------- DATA (memory mein) ----------
vector<Account> accounts;            // saare accounts
vector<Transaction> transactions;    // saari transactions
int nextAccNo = 1001;                // agla account number
int nextTxnId = 1;                   // agli transaction ka id


// =====================================================
//  HELPER FUNCTIONS
// =====================================================

// Text ke shuru aur end ke extra spaces hatata hai
string trim(const string& text) {
    size_t start = 0;
    while (start < text.size() && isspace((unsigned char)text[start])) start++;
    size_t end = text.size();
    while (end > start && isspace((unsigned char)text[end - 1])) end--;
    return text.substr(start, end - start);
}

// User ke text ko file mein save karne layak banata hai
// ('|' file ka separator hai, isliye use '/' bana dete hain)
string cleanText(string text) {
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '|') text[i] = '/';
        if (text[i] == '\n' || text[i] == '\r') text[i] = ' ';
    }
    return trim(text);
}

// JSON mein bhejne ke liye text ko safe banata hai
string jsonText(const string& text) {
    string out = "";
    for (size_t i = 0; i < text.size(); i++) {
        char c = text[i];
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n' || c == '\r' || c == '\t') out += " ";
        else out += c;
    }
    return out;
}

// Text mein sirf digits hain aur lambai bilkul "length" hai? (phone, PIN ke liye)
bool isDigits(const string& text, size_t length) {
    if (text.size() != length) return false;
    for (size_t i = 0; i < text.size(); i++) {
        if (!isdigit((unsigned char)text[i])) return false;
    }
    return true;
}

// Text ko number banata hai. Sirf digits aur maximum 9 digit allowed
bool toNumber(const string& input, long long& value) {
    string text = trim(input);
    if (text.empty() || text.size() > 9) return false;
    for (size_t i = 0; i < text.size(); i++) {
        if (!isdigit((unsigned char)text[i])) return false;
    }
    value = stoll(text);
    return true;
}

// Abhi ki date aur time, jaise "2026-10-19 14:30"
string today() {
    time_t now = time(NULL);
    char buffer[30];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", localtime(&now));
    return string(buffer);
}

// Galti ka jawab bhejta hai
void sendError(Response& res, int status, const string& message) {
    res.status = status;
    res.set_content("{\"error\":\"" + jsonText(message) + "\"}", "application/json");
}

// Message aur naya balance bhejta hai
void sendBalance(Response& res, const string& message, long long balance) {
    res.set_content("{\"message\":\"" + jsonText(message) + "\",\"balance\":" +
                    to_string(balance) + "}", "application/json");
}

// Account number se account dhundta hai (nahi mila to NULL)
Account* findAccount(int accNo) {
    for (size_t i = 0; i < accounts.size(); i++) {
        if (accounts[i].accNo == accNo) return &accounts[i];
    }
    return NULL;
}

// Nayi transaction ko list mein jodta hai
void addTransaction(int accNo, const string& type, long long amount,
                    long long balanceAfter, const string& note) {
    Transaction t;
    t.id = nextTxnId;
    nextTxnId++;
    t.accNo = accNo;
    t.type = type;
    t.amount = amount;
    t.balanceAfter = balanceAfter;
    t.date = today();
    t.note = note;
    transactions.push_back(t);
}

// Login check: account number aur PIN sahi hain ya nahi
// Sahi ho to account ka pointer deta hai, galat ho to error bhejke NULL deta hai
Account* checkLogin(const Request& req, Response& res) {
    long long accNo;
    string pin = trim(req.get_param_value("pin"));

    if (!toNumber(req.get_param_value("accNo"), accNo)) {
        sendError(res, 401, "Invalid account number or PIN");
        return NULL;
    }

    Account* acc = findAccount((int)accNo);
    if (acc == NULL || acc->pin != pin) {
        sendError(res, 401, "Invalid account number or PIN");
        return NULL;
    }
    if (acc->status != 1) {
        sendError(res, 403, "This account has been closed");
        return NULL;
    }
    return acc;
}

// Amount check: poora number, 0 se bada, aur bahut bada nahi
bool getAmount(const Request& req, Response& res, long long& amount) {
    if (!toNumber(req.get_param_value("amount"), amount) || amount <= 0) {
        sendError(res, 400, "Please enter a valid amount (greater than 0)");
        return false;
    }
    if (amount > MAX_AMOUNT) {
        sendError(res, 400, "Amount cannot be more than Rs. 1,00,00,000 at a time");
        return false;
    }
    return true;
}

// Admin ka password check
bool checkAdmin(const Request& req, Response& res) {
    if (req.get_param_value("password") != ADMIN_PASSWORD) {
        sendError(res, 401, "Wrong admin password");
        return false;
    }
    return true;
}


// =====================================================
//  MAIN
// =====================================================
int main() {
    // start hote hi files se data load karo
    accounts = loadAccounts();
    transactions = loadTransactions();

    for (size_t i = 0; i < accounts.size(); i++) {
        if (accounts[i].accNo >= nextAccNo) nextAccNo = accounts[i].accNo + 1;
    }
    for (size_t i = 0; i < transactions.size(); i++) {
        if (transactions[i].id >= nextTxnId) nextTxnId = transactions[i].id + 1;
    }

    Server svr;

    // frontend folder ki files (html, css, js) browser ko dene ke liye
    svr.set_mount_point("/", "./frontend");

    // ----- CREATE ACCOUNT -----
    svr.Post("/api/create", [](const Request& req, Response& res) {
        string name = cleanText(req.get_param_value("name"));
        string phone = trim(req.get_param_value("phone"));
        string pin = trim(req.get_param_value("pin"));
        long long deposit;

        if (name == "" || name.size() > 50) {
            sendError(res, 400, "Name is required (maximum 50 characters)");
            return;
        }
        if (!isDigits(phone, 10)) {
            sendError(res, 400, "Phone number must be exactly 10 digits");
            return;
        }
        if (!isDigits(pin, 4)) {
            sendError(res, 400, "PIN must be exactly 4 digits");
            return;
        }
        if (!toNumber(req.get_param_value("deposit"), deposit) || deposit < MIN_BALANCE) {
            sendError(res, 400, "Opening deposit must be at least Rs. 500");
            return;
        }
        if (deposit > MAX_AMOUNT) {
            sendError(res, 400, "Opening deposit is too large");
            return;
        }

        Account acc;
        acc.accNo = nextAccNo;
        nextAccNo++;
        acc.name = name;
        acc.phone = phone;
        acc.pin = pin;
        acc.balance = deposit;
        acc.status = 1;
        accounts.push_back(acc);

        addTransaction(acc.accNo, "Deposit", deposit, deposit, "Opening deposit");

        saveAccounts(accounts);
        saveTransactions(transactions);
        res.set_content("{\"accNo\":" + to_string(acc.accNo) + "}", "application/json");
    });

    // ----- LOGIN -----
    svr.Post("/api/login", [](const Request& req, Response& res) {
        Account* acc = checkLogin(req, res);
        if (acc == NULL) return;
        res.set_content("{\"name\":\"" + jsonText(acc->name) + "\"}", "application/json");
    });

    // ----- BALANCE -----
    svr.Post("/api/balance", [](const Request& req, Response& res) {
        Account* acc = checkLogin(req, res);
        if (acc == NULL) return;
        res.set_content("{\"balance\":" + to_string(acc->balance) + "}", "application/json");
    });

    // ----- DEPOSIT -----
    svr.Post("/api/deposit", [](const Request& req, Response& res) {
        Account* acc = checkLogin(req, res);
        if (acc == NULL) return;

        long long amount;
        if (!getAmount(req, res, amount)) return;

        acc->balance += amount;
        addTransaction(acc->accNo, "Deposit", amount, acc->balance, "Cash deposit");

        saveAccounts(accounts);
        saveTransactions(transactions);
        sendBalance(res, "Deposit successful", acc->balance);
    });

    // ----- WITHDRAW -----
    svr.Post("/api/withdraw", [](const Request& req, Response& res) {
        Account* acc = checkLogin(req, res);
        if (acc == NULL) return;

        long long amount;
        if (!getAmount(req, res, amount)) return;

        // withdraw ke baad bhi kam se kam Rs. 500 bachne chahiye
        if (acc->balance - amount < MIN_BALANCE) {
            sendError(res, 400, "Insufficient balance. Minimum Rs. 500 must remain in your account.");
            return;
        }

        acc->balance -= amount;
        addTransaction(acc->accNo, "Withdraw", amount, acc->balance, "Cash withdrawal");

        saveAccounts(accounts);
        saveTransactions(transactions);
        sendBalance(res, "Withdrawal successful", acc->balance);
    });

    // ----- TRANSFER -----
    svr.Post("/api/transfer", [](const Request& req, Response& res) {
        Account* sender = checkLogin(req, res);
        if (sender == NULL) return;

        long long amount;
        if (!getAmount(req, res, amount)) return;

        long long toNo;
        if (!toNumber(req.get_param_value("to"), toNo)) {
            sendError(res, 400, "Please enter a valid receiver account number");
            return;
        }
        if (toNo == sender->accNo) {
            sendError(res, 400, "You cannot transfer money to your own account");
            return;
        }

        Account* receiver = findAccount((int)toNo);
        if (receiver == NULL || receiver->status != 1) {
            sendError(res, 404, "Receiver account not found");
            return;
        }

        if (sender->balance - amount < MIN_BALANCE) {
            sendError(res, 400, "Insufficient balance. Minimum Rs. 500 must remain in your account.");
            return;
        }

        // paisa ek account se kat ke doosre mein jaata hai
        sender->balance -= amount;
        receiver->balance += amount;

        addTransaction(sender->accNo, "Transfer Out", amount, sender->balance,
                       "To account " + to_string(receiver->accNo));
        addTransaction(receiver->accNo, "Transfer In", amount, receiver->balance,
                       "From account " + to_string(sender->accNo));

        saveAccounts(accounts);
        saveTransactions(transactions);
        sendBalance(res, "Transfer successful", sender->balance);
    });

    // ----- TRANSACTION HISTORY -----
    svr.Post("/api/history", [](const Request& req, Response& res) {
        Account* acc = checkLogin(req, res);
        if (acc == NULL) return;

        string j = "[";
        bool first = true;
        for (size_t i = 0; i < transactions.size(); i++) {
            const Transaction& t = transactions[i];
            if (t.accNo != acc->accNo) continue;   // sirf is account ki transactions

            if (!first) j += ",";
            first = false;
            j += "{\"id\":" + to_string(t.id) +
                 ",\"date\":\"" + jsonText(t.date) + "\"" +
                 ",\"type\":\"" + jsonText(t.type) + "\"" +
                 ",\"amount\":" + to_string(t.amount) +
                 ",\"balance_after\":" + to_string(t.balanceAfter) +
                 ",\"note\":\"" + jsonText(t.note) + "\"}";
        }
        j += "]";
        res.set_content(j, "application/json");
    });

    // ----- CHANGE PIN -----
    svr.Post("/api/changepin", [](const Request& req, Response& res) {
        Account* acc = checkLogin(req, res);   // "pin" mein purana PIN aata hai
        if (acc == NULL) return;

        string newPin = trim(req.get_param_value("newpin"));
        if (!isDigits(newPin, 4)) {
            sendError(res, 400, "New PIN must be exactly 4 digits");
            return;
        }
        if (newPin == acc->pin) {
            sendError(res, 400, "New PIN must be different from the old PIN");
            return;
        }

        acc->pin = newPin;
        saveAccounts(accounts);
        res.set_content("{\"message\":\"PIN changed successfully\"}", "application/json");
    });

    // ----- ADMIN: SAARE ACCOUNTS -----
    svr.Post("/api/admin/accounts", [](const Request& req, Response& res) {
        if (!checkAdmin(req, res)) return;

        string j = "[";
        bool first = true;
        for (size_t i = 0; i < accounts.size(); i++) {
            if (accounts[i].status != 1) continue;   // band accounts nahi dikhte

            if (!first) j += ",";
            first = false;
            j += "{\"accNo\":" + to_string(accounts[i].accNo) +
                 ",\"name\":\"" + jsonText(accounts[i].name) + "\"" +
                 ",\"phone\":\"" + jsonText(accounts[i].phone) + "\"" +
                 ",\"balance\":" + to_string(accounts[i].balance) + "}";
        }
        j += "]";
        res.set_content(j, "application/json");
    });

    // ----- ADMIN: ACCOUNT BAND KARO -----
    svr.Post("/api/admin/delete", [](const Request& req, Response& res) {
        if (!checkAdmin(req, res)) return;

        long long accNo;
        if (!toNumber(req.get_param_value("accNo"), accNo)) {
            sendError(res, 400, "Invalid account number");
            return;
        }

        Account* acc = findAccount((int)accNo);
        if (acc == NULL || acc->status != 1) {
            sendError(res, 404, "Account not found");
            return;
        }

        acc->status = 0;   // 0 = band, 1 = chalu
        saveAccounts(accounts);
        res.set_content("{\"message\":\"Account " + to_string(acc->accNo) +
                        " closed successfully\"}", "application/json");
    });

    cout << "Bank Management System is running.\n";
    cout << "Open in browser: http://localhost:8080\n";
    cout << "Press Ctrl+C to stop.\n";

    if (!svr.listen("127.0.0.1", 8080)) {
        cout << "Could not start server. Port 8080 may be in use.\n";
    }
    return 0;
}