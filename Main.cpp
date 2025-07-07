// Final single-file Huffman encoder/decoder with compression >20% for large files
// Combines huffmantool.h, cfp.h, scanner.h, and cli_compression.cpp logic into one .cpp file

#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <queue>
#include <string>
#include <bitset>
#include <iomanip>
#include <chrono>

using namespace std;

// -------------------------- charFreqPair and comparator --------------------------
class charFreqPair {
    char ch;
    int freq;
public:
    charFreqPair *left, *right;
    charFreqPair() {}
    charFreqPair(char const &ch, int const &freq) : ch(ch), freq(freq), left(NULL), right(NULL) {}
    char getChar() const { return ch; }
    int getFreq() const { return freq; }
    void setFreq(int f) { freq = f; }
    ~charFreqPair() {
        delete left;
        delete right;
    }
};

class pairComparator {
public:
    int operator()(charFreqPair *const &a, charFreqPair *const &b) {
        return a->getFreq() > b->getFreq();
    }
};

// -------------------------- Scanner utility --------------------------
class scanner {
public:
    int getFileSize(const string &filename) {
        ifstream reader(filename, ios::binary | ios::ate);
        if (!reader.is_open()) {
            cerr << "ERROR: Cannot open file " << filename << "\n";
            return -1;
        }
        return static_cast<int>(reader.tellg());
    }
};

// -------------------------- HuffmanTool Class --------------------------
class huffmantool {
    void traverse(charFreqPair const *node, unordered_map<char, string> &charKeyMap, const string &s) {   // traverse tree and build charKeyMap
        if (!node->left && !node->right) {
            charKeyMap[node->getChar()] = s;
            return;
        }
        traverse(node->left, charKeyMap, s + "0");
        traverse(node->right, charKeyMap, s + "1");
    }

    void traverse(charFreqPair const *node, unordered_map<string, char> &keyCharMap, const string &s) { //
        if (!node->left && !node->right) {
            keyCharMap[s] = node->getChar();
            return;
        }
        traverse(node->left, keyCharMap, s + "0");
        traverse(node->right, keyCharMap, s + "1");
    }

    charFreqPair* readTree(ifstream &in) {
        char nodeType;
        in >> noskipws >> nodeType;
        if (nodeType == '1') {
            char ch;
            in >> noskipws >> ch;
            return new charFreqPair(ch, 0);
        }
        auto *node = new charFreqPair('~', 0);
        node->left = readTree(in);
        node->right = readTree(in);
        return node;
    }

    void writeTree(ofstream &out, const charFreqPair *node) { // write tree to output file
        if (!node->left && !node->right) {
            out << '1' << node->getChar();
            return;
        }
        out << '0';
        writeTree(out, node->left);
        writeTree(out, node->right);
    }

    int lposSlash(const string &path) { // last position of slash in path
        int pos = -1;
        for (size_t i = 0; i < path.length(); i++)
            if (path[i] == '/') pos = i;
        return pos;
    }

public:
    string compressFile(string sourceFile, string compressedFile = "") {
        if (compressedFile == "") {
            int pos = lposSlash(sourceFile);
            compressedFile = sourceFile.substr(0, pos + 1) + "compressed_" + sourceFile.substr(pos + 1);
        }
        ifstream reader(sourceFile, ios::binary);
        if (!reader.is_open()) {
            cerr << "Cannot open file: " << sourceFile << endl;
            return "";
        }

        unordered_map<char, int> freq;
        vector<char> content;
        char ch;
        while (reader.get(ch)) {
            freq[ch]++;
            content.push_back(ch);
        }
        reader.close();

        priority_queue<charFreqPair*, vector<charFreqPair*>, pairComparator> pq;
        for (auto &p : freq)
            pq.push(new charFreqPair(p.first, p.second));

        while (pq.size() > 1) {
            charFreqPair *a = pq.top(); pq.pop();
            charFreqPair *b = pq.top(); pq.pop();
            auto *node = new charFreqPair('~', a->getFreq() + b->getFreq());
            node->left = a;
            node->right = b;
            pq.push(node);
        }

        charFreqPair *root = pq.top();
        unordered_map<char, string> charKeyMap;
        traverse(root, charKeyMap, "");

        ofstream writer(compressedFile, ios::binary);
        writeTree(writer, root);

        int numChars = content.size();
        writer.write(reinterpret_cast<const char*>(&numChars), sizeof(numChars));

        string buffer;
        for (char c : content)
            buffer += charKeyMap[c];

        while (buffer.size() % 8 != 0)
            buffer += '0';

        for (size_t i = 0; i < buffer.size(); i += 8) {
            bitset<8> b(buffer.substr(i, 8));
            writer.put(static_cast<char>(b.to_ulong()));
        }

        writer.close();
        delete root;
        return compressedFile;
    }

    string decompressFile(string compressedFile, string retrievedFile = "") {
        if (retrievedFile == "") {
            int pos = lposSlash(compressedFile);
            retrievedFile = compressedFile.substr(0, pos + 1) + "decompressed_";
            if (compressedFile.substr(pos + 1, 11) == "compressed_")
                retrievedFile += compressedFile.substr(pos + 11 + 1);
            else
                retrievedFile += compressedFile.substr(pos + 1);
        }
        ifstream reader(compressedFile, ios::binary);
        if (!reader.is_open()) {
            cerr << "Cannot open file: " << compressedFile << endl;
            return "";
        }
        charFreqPair *root = readTree(reader);
        unordered_map<string, char> keyCharMap;
        traverse(root, keyCharMap, "");

        int totalChars;
        reader.read(reinterpret_cast<char*>(&totalChars), sizeof(totalChars));

        string key = "";
        string output = "";
        char ch;
        while (reader.get(ch) && output.size() < static_cast<size_t>(totalChars)) {
            string bin = bitset<8>(static_cast<unsigned char>(ch)).to_string();
            for (char b : bin) {
                key += b;
                if (keyCharMap.count(key)) {
                    output += keyCharMap[key];
                    key.clear();
                    if (output.size() == static_cast<size_t>(totalChars)) break;
                }
            }
        }
        reader.close();

        ofstream writer(retrievedFile, ios::binary);
        writer.write(output.c_str(), output.size());
        writer.close();
        delete root;
        return retrievedFile;
    }

    void benchmark(string sourceFile) {
        scanner sc;
        auto start_compress = chrono::high_resolution_clock::now();
        string compressedFile = compressFile(sourceFile);
        auto end_compress = chrono::high_resolution_clock::now();
        string decompressedFile = decompressFile(compressedFile);
        auto end_decompress = chrono::high_resolution_clock::now();

        int orig = sc.getFileSize(sourceFile);
        int comp = sc.getFileSize(compressedFile);
        int decomp = sc.getFileSize(decompressedFile);

        cout << "\nFile sizes (in bytes):\n";
        cout << left << setw(15) << "Original:" << orig << "\n";
        cout << left << setw(15) << "Compressed:" << comp << "\n";
        cout << left << setw(15) << "Decompressed:" << decomp << "\n";

        float compression = 100.0 - ((float)comp / orig * 100.0);
        cout << fixed << setprecision(2);
        cout << "Compression: " << compression << "%\n";
        cout << "Time Compress: " << chrono::duration_cast<chrono::milliseconds>(end_compress - start_compress).count() << " ms\n";
        cout << "Time Decompress: " << chrono::duration_cast<chrono::milliseconds>(end_decompress - end_compress).count() << " ms\n";
    }
};

// -------------------------- Main --------------------------
int main() {
    huffmantool ht;
    string file;
    cout << "Enter path to file: ";
    cin >> file;
    ht.benchmark(file);
    return 0;
}
