#include <chrono>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <thread>

#include "uci.h"
using namespace std;

const string STARTPOS = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

void setPosition(string &input, vector<string> &inputVector, Board &board);
vector<string> split(const string &s, char d);
Board fenToBoard(string s);

volatile bool isStop = true;

int main() {
    initKindergartenTables();
    initZobristTable();

    string input;
    vector<string> inputVector;
    string name = "UCI Chess Engine";
    string version = "0";
    string author = "Jeffrey An and Michael An";
    thread searchThread;
    Move bestMove = NULL_MOVE;
    
    Board board = fenToBoard(STARTPOS);
    
    cout << name << " " << version << " by " << author << endl;
    
    while (input != "quit") {
        getline(cin, input);
        inputVector = split(input, ' ');
        cin.clear();
        
        if (input == "uci") {
            cout << "id name " << name << " " << version << endl;
            cout << "id author " << author << endl;
            // make variables for default, min, and max values for hash in MB
            cout << "option name Hash type spin default " << 16 << " min " << 1 << " max " << 1024 << endl;
            cout << "uciok" << endl;
        }
        
        if (input == "isready") cout << "readyok" << endl;
        
        if (input == "ucinewgame") {
            board = fenToBoard(STARTPOS);
            clearTranspositionTable();
        }
        
        if (input.substr(0, 8) == "position") setPosition(input, inputVector, board);
        
        if (input.substr(0, 2) == "go" && isStop) {
            int mode = DEPTH, value = 1;
            
            if (input.find("movetime") != string::npos && inputVector.size() > 2) {
                mode = TIME;
                value = stoi(inputVector.at(2));
            }
            
            if (input.find("depth") != string::npos && inputVector.size() > 2) {
                mode = DEPTH;
                value = stoi(inputVector.at(2));
            }
            
            if (input.find("infinite") != string::npos) {
                mode = DEPTH;
                value = MAX_DEPTH;
            }
            
            if (input.find("wtime") != string::npos) {
                mode = TIME;
                int color = board.getPlayerToMove();
                
                if (inputVector.size() == 5) {
                    if (color == WHITE) value = stoi(inputVector.at(2));
                    else value = stoi(inputVector.at(4));
                }
                if (inputVector.size() == 9) {
                    if (color == WHITE) value = stoi(inputVector.at(2)) + 40 * stoi(inputVector.at(6));
                    else value = stoi(inputVector.at(4)) + 40 * stoi(inputVector.at(8));
                }
                // Primitive time management: use on average 1/40 of remaining time with a 200 ms buffer zone
                value = (value - 200) / 40;
            }
            
            bestMove = NULL_MOVE;
            isStop = false;
            searchThread = thread(getBestMove, &board, mode, value, &bestMove);
            searchThread.detach();
        }
        
        if (input == "stop") {
            // TODO make this block until search stops
            isStop = true;
        }
        
        if (input == "mailbox") {
            int *mailbox = board.getMailbox();
            for (unsigned i = 0; i < 64; i++) {
                cerr << mailbox[i] << ' ';
                if (i % 8 == 7) cerr << endl;
            }
            delete[] mailbox;
        }

        if (input.substr(0, 5) == "perft" && inputVector.size() == 2) {
            int depth = stoi(inputVector.at(1));

            Board b;
            uint64_t captures = 0;
            using namespace std::chrono;
            auto start_time = high_resolution_clock::now();

            cerr << "Nodes: " << perft(b, 1, depth, captures) << endl;
            cerr << "Captures: " << captures << endl;

            auto end_time = high_resolution_clock::now();
            duration<double> time_span = duration_cast<duration<double>>(
                end_time-start_time);

            cerr << time_span.count() << endl;
        }
        
        // According to UCI protocol, inputs that do not make sense are ignored
    }
}

void setPosition(string &input, vector<string> &inputVector, Board &board) {
    string pos;

    if (input.find("startpos") != string::npos)
        pos = STARTPOS;
    
    if (input.find("fen") != string::npos) {
        if (inputVector.size() < 7 || inputVector.at(6) == "moves") {
            pos = inputVector.at(2) + ' ' + inputVector.at(3) + ' ' + inputVector.at(4) + ' '
                + inputVector.at(5) + " 0 1";
        }
        else {
            pos = inputVector.at(2) + ' ' + inputVector.at(3) + ' ' + inputVector.at(4) + ' '
                + inputVector.at(5) + ' ' + inputVector.at(6) + ' ' + inputVector.at(7);
        }
    }
    
    board = fenToBoard(pos);
    
    if (input.find("moves") != string::npos) {
        string moveList = input.substr(input.find("moves") + 6);
        vector<string> moveVector = split(moveList, ' ');
        
        for (unsigned i = 0; i < moveVector.size(); i++) {
            // moveString contains the move in long algebraic notation
            string moveString = moveVector.at(i);
            
            char startFile = moveString.at(0);
            char startRank = moveString.at(1);
            char endFile = moveString.at(2);
            char endRank = moveString.at(3);
            int startSq = 8 * (startRank - '1') + (startFile - 'a');
            int endSq = 8 * (endRank - '1') + (endFile - 'a');
            
            int *mailbox = board.getMailbox();
            int piece = mailbox[startSq] % 6;
            bool isCapture = ((mailbox[endSq] != -1)
                    || (piece == PAWNS && ((startSq - endSq) & 1)));
            delete[] mailbox;
            
            bool isCastle = (piece == KINGS && abs(endSq - startSq) == 2);
            string promotionString = " nbrq";
            int promotion = (moveString.length() == 5)
                ? promotionString.find(moveString.at(4)) : 0;
            
            Move m = encodeMove(startSq, endSq, piece, isCapture);
            m = setCastle(m, isCastle);
            m = setPromotion(m, promotion);
            
            board.doMove(m, board.getPlayerToMove());
        }
    }
}

// Splits a string s with delimiter d.
vector<string> split(const string &s, char d) {
    vector<string> v;
    stringstream ss(s);
    string item;
    while (getline(ss, item, d)) {
        v.push_back(item);
    }
    return v;
}

Board fenToBoard(string s) {
    vector<string> components = split(s, ' ');
    vector<string> rows = split(components.at(0), '/');
    int mailbox[64];
    int sqCounter = 0;
    string pieceString = "PNBRQKpnbrqk";
    
    // iterate through rows backwards (because mailbox goes a1 -> h8), converting into mailbox format
    for (int elem = 7; elem >= 0; elem--) {
        string rowAtElem = rows.at(elem);
        
        for (unsigned col = 0; col < rowAtElem.length(); col++) {
            char sq = rowAtElem.at(col);
            do {
                mailbox[sqCounter] = pieceString.find(sq);
                sqCounter++;
                sq--;
            }
            while ('0' < sq && sq < '8');
        }
    }
    
    int playerToMove = (components.at(1) == "w") ? WHITE : BLACK;
    bool whiteCanKCastle = (components.at(2).find("K") != string::npos);
    bool whiteCanQCastle = (components.at(2).find("Q") != string::npos);
    bool blackCanKCastle = (components.at(2).find("k") != string::npos);
    bool blackCanQCastle = (components.at(2).find("q") != string::npos);
    int epCaptureFile = (components.at(3) == "-") ? NO_EP_POSSIBLE
        : components.at(3).at(0) - 'a';
    int fiftyMoveCounter = stoi(components.at(4));
    int moveNumber = stoi(components.at(5));
    return Board(mailbox, whiteCanKCastle, blackCanKCastle, whiteCanQCastle,
            blackCanQCastle, epCaptureFile, fiftyMoveCounter, moveNumber,
            playerToMove);
}
