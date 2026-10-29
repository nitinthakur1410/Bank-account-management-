#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <vector>
#include <fstream>
#include <cstdlib>
using namespace std;

// Ek bank account
struct Account {
    int accNo;          // account number
    string name;
    string phone;
    string pin;         // 4 digit PIN
    long long balance;  // paisa (Rs.)
    int status;         // 1 = chalu, 0 = band
};

// Ek transaction (deposit, withdraw ya transfer)
struct Transaction {
    int id;
    int accNo;              // kis account ki transaction hai
    string type;            // Deposit, Withdraw, Transfer In, Transfer Out
    long long amount;
    long long balanceAfter; // transaction ke baad balance
    string date;
    string note;
};

// Data in files mein save hoga (program main folder se chalana hai)
const string ACCOUNTS_FILE = "database/accounts.txt";
const string TRANSACTIONS_FILE = "database/transactions.txt";

// Ek line ko '|' se todkar alag-alag parts banata hai
vector<string> splitLine(const string& line) {
    vector<string> parts;
    string current = "";
    for (size_t i = 0; i < line.size(); i++) {
        if (line[i] == '|') {
            parts.push_back(current);
            current = "";
        } else if (line[i] != '\r') {
            current += line[i];
        }
    }
    parts.push_back(current);
    return parts;
}

// ---------- ACCOUNTS ----------

// File se saare accounts padhta hai
vector<Account> loadAccounts() {
    vector<Account> list;
    ifstream file(ACCOUNTS_FILE);

    if (!file) {
        return list;   // file nahi mili to khali list
    }

    string line;
    while (getline(file, line)) {
        vector<string> p = splitLine(line);
        if (p.size() < 6) {
            continue;   // kharab ya khali line chhod do
        }

        Account a;
        a.accNo = atoi(p[0].c_str());
        a.name = p[1];
        a.phone = p[2];
        a.pin = p[3];
        a.balance = atoll(p[4].c_str());
        a.status = atoi(p[5].c_str());
        list.push_back(a);
    }

    file.close();
    return list;
}

// Saare accounts ko file mein likhta hai
void saveAccounts(const vector<Account>& list) {
    ofstream file(ACCOUNTS_FILE);

    for (size_t i = 0; i < list.size(); i++) {
        const Account& a = list[i];
        file << a.accNo << "|"
             << a.name << "|"
             << a.phone << "|"
             << a.pin << "|"
             << a.balance << "|"
             << a.status << "\n";
    }

    file.close();
}

// ---------- TRANSACTIONS ----------

// File se saari transactions padhta hai
vector<Transaction> loadTransactions() {
    vector<Transaction> list;
    ifstream file(TRANSACTIONS_FILE);

    if (!file) {
        return list;
    }

    string line;
    while (getline(file, line)) {
        vector<string> p = splitLine(line);
        if (p.size() < 7) {
            continue;
        }

        Transaction t;
        t.id = atoi(p[0].c_str());
        t.accNo = atoi(p[1].c_str());
        t.type = p[2];
        t.amount = atoll(p[3].c_str());
        t.balanceAfter = atoll(p[4].c_str());
        t.date = p[5];
        t.note = p[6];
        list.push_back(t);
    }

    file.close();
    return list;
}

// Saari transactions ko file mein likhta hai
void saveTransactions(const vector<Transaction>& list) {
    ofstream file(TRANSACTIONS_FILE);

    for (size_t i = 0; i < list.size(); i++) {
        const Transaction& t = list[i];
        file << t.id << "|"
             << t.accNo << "|"
             << t.type << "|"
             << t.amount << "|"
             << t.balanceAfter << "|"
             << t.date << "|"
             << t.note << "\n";
    }

    file.close();
}

#endif