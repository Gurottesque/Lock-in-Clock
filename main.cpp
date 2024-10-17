#define WIN32_LEAN_AND_MEAN    
#pragma comment(linker, "/Merge:.rdata=.text")
#include <SFML/Graphics.hpp>
#include "stdio.h"
#include <cmath>
#include <cstdio>
#include <memory>
#include <fstream>
#include <iostream>
#include <string>
#include <cstdlib>
#include <vector>
#include <sstream>
#include <algorithm>
#include <regex>
#include <unordered_set>
#include <future>
#include <thread>

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
    #include <conio.h>
    #include <ws2tcpip.h>
    #include <tlhelp32.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <netdb.h>
    #include <unistd.h>
#endif




std::string formatTime(int);
void setText(sf::Text& text, float posx, float posy, sf::Font& font, bool isSec);

int loadIPAddresses();
void blockIP(const std::string& IPAddress);
void unblockIP(const std::string& IPAddress);
std::unordered_set<std::string> IPAddresses;

int loadProcessesToBlock();
PROCESSENTRY32 entry;
HANDLE snapshot;
std::vector<std::string> processToBlock;
void blockProcesses();

int main()
{
    sf::RenderWindow window(sf::VideoMode(400, 600), "GrindClock");

    window.setFramerateLimit(60);
    sf::Clock clock;
    sf::Text hours_counter, minutes_counter, seconds_counter;
    sf::Font font;
    std::thread loadThread(loadIPAddresses);
    loadProcessesToBlock();

    if (!font.loadFromFile("font.ttf"))
    {
        printf("what\n");
        return -1;
    }

    sf::Texture _sisyphus_texture;
    if (!_sisyphus_texture.loadFromFile("sisyphus.png"))
    {
        std::cerr << "what" << std::endl;
    }

    sf::Sprite sisyphus;
    sisyphus.setTexture(_sisyphus_texture);
    sisyphus.scale(sf::Vector2f(0.5f, 0.48f));
    sisyphus.setPosition(110,110);

    setText(hours_counter, 20, 350, font, false);
    setText(minutes_counter, 150, 350, font, false);
    setText(seconds_counter, 290, 350, font, true);

    bool isClockRunning = false;

    while (window.isOpen())
    {
        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
                window.close();

            if (event.type == sf::Event::MouseButtonPressed)
            {
                if (event.mouseButton.button == sf::Mouse::Left){
                    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
                    sf::FloatRect spriteBounds = sisyphus.getGlobalBounds();


                    if (spriteBounds.contains(mousePos)) {
                        if (isClockRunning) { isClockRunning = false; for (auto& ip : IPAddresses) { unblockIP(ip); } } 
                        else { clock.restart(); isClockRunning = true; for (auto& ip : IPAddresses) { blockIP(ip); } }
                    }

                }
            }


        }

        window.clear();

        if(isClockRunning){

            blockProcesses();

            sf::Time elapsed = clock.getElapsedTime();
            seconds_counter.setString(formatTime(((int)elapsed.asSeconds()) % 60));
            minutes_counter.setString(formatTime(((int)elapsed.asSeconds()) / 60 % 60) + ":");
            hours_counter.setString(formatTime(((int)elapsed.asSeconds()) / 3600) + ":");
        }
        window.draw(hours_counter);
        window.draw(minutes_counter);
        window.draw(seconds_counter);
        window.draw(sisyphus);

        window.display();
    }

    return 0;
}

std::string formatTime(int time) {
    return (time < 10 ? "0" : "") + std::to_string(time);
}



void setText(sf::Text& text, float posx, float posy, sf::Font& font, bool isSec){
    text.setFont(font);
    text.setPosition(posx, posy);
    text.setCharacterSize(48);
    text.setString(std::string("00") + (isSec ? "" : ":"));
    text.setFillColor(sf::Color::White);
}

void blockIP(const std::string& IPAddress) {
    std::string command;

    #ifdef _WIN32
        command = "netsh advfirewall firewall add rule name=\"Block\" dir=OUT action=BLOCK remoteip=" + IPAddress;
    #elif __linux__
        command = "sudo iptables -A OUTPUT -d " + IPAddress + " -j DROP";
    #endif

    system(command.c_str());

}

void unblockIP(const std::string& IPAddress) {
    std::string command;

    #ifdef _WIN32
        command = "netsh advfirewall firewall delete rule name=\"Block\" dir=OUT remoteip=" + IPAddress;
    #elif __linux__
        command = "sudo iptables -D OUTPUT -d " + IPAddress + " -j DROP";
    #endif

    system(command.c_str());
}

void lookupDomain(const std::string& domain) {
    std::string command = "nslookup " + domain + " 8.8.8.8";
    std::regex ip_regex(R"(\b(?:\d{1,3}\.){3}\d{1,3}\b|([a-fA-F0-9]{1,4}:){7}[a-fA-F0-9]{1,4}|\b([a-fA-F0-9]{1,4}:){1,7}:([a-fA-F0-9]{1,4})?\b)");
    std::smatch match;

    for (int i = 0; i < 5; i++) {  // Retrying up to 5 times
        #ifdef _WIN32
            FILE* pipe = _popen(command.c_str(), "r");
        #elif __linux__
            FILE* pipe = popen(command.c_str(), "r");
        #endif

        if (!pipe) {
            std::cerr << "Error executing nslookup for " << domain << std::endl;
            return;
        }

        std::string output;
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            output += buffer;
        }

        #ifdef _WIN32
            _pclose(pipe);
        #elif __linux__
            pclose(pipe);
        #endif

        while (std::regex_search(output, match, ip_regex)) {
            std::string ip = match.str(0);
            if (ip != "8.8.8.8") {
                IPAddresses.insert(ip);  // Only insert unique IPs
            }
            output = match.suffix().str();
        }
    }
}

int loadIPAddresses() {
    std::ifstream urlFile("url_list.txt");
    if (urlFile.is_open()) {

        std::vector<std::string> domains;
        std::string domain;
        while (std::getline(urlFile, domain)) {
            domains.push_back(domain);
        }
        urlFile.close();

        std::vector<std::future<void>> futures;

        for (const auto& dom : domains) {
            futures.push_back(std::async(std::launch::async, lookupDomain, dom));
        }

        // Wait for all threads to finish
        for (auto& future : futures) {
            future.get();
        }
    }
    else
    {
		std::ofstream newFile("url_list.txt");
		if (newFile.is_open()) //is the file open?
		{
			newFile << ""; //write the file
			newFile.close(); //close it
		}
	}

    return 0;
}

int loadProcessesToBlock()
{
	std::string processRetrieved;
	std::ifstream blockedProcesses("exes_list.txt");
	if (blockedProcesses.is_open())
	{
		while (std::getline(blockedProcesses, processRetrieved))
		{
			if (processRetrieved != "")
			{
				processToBlock.push_back(processRetrieved);
				processRetrieved = "";
			}
		}
		blockedProcesses.close();

	}
	else
	{
		std::ofstream newFile("exes_list.txt");
		if (newFile.is_open()) //is the file open?
		{
			newFile << ""; //write the file
			newFile.close(); //close it
		}
	}

	return 0;


}

void blockProcesses(){
    #ifdef _WIN32
        PROCESSENTRY32 entry;
        HANDLE snapshot;

        entry.dwSize = sizeof(PROCESSENTRY32);
        SleepEx(100, 0);
        snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

        for (const auto& e : processToBlock) {
            if (Process32First(snapshot, &entry) == TRUE) {
                do {
                    if (strcmp((char*)entry.szExeFile, e.c_str()) == 0) {
                        HANDLE processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, entry.th32ProcessID);
                        TerminateProcess(processHandle, 9);
                        CloseHandle(processHandle);
                    }
                } while (Process32Next(snapshot, &entry) == TRUE);
            }
        }
        CloseHandle(snapshot);
    #else
        for (const auto& e : processToBlock) {
            DIR* dir;
            struct dirent* ent;
            if ((dir = opendir("/proc")) != NULL) {
                while ((ent = readdir(dir)) != NULL) {
                    // Ignore entries that aren't directories or numbers
                    if (ent->d_type == DT_DIR && isdigit(ent->d_name[0])) {
                        std::string path = std::string("/proc/") + ent->d_name + "/exe";
                        char exePath[1024];
                        ssize_t len = readlink(path.c_str(), exePath, sizeof(exePath) - 1);
                        if (len != -1) {
                            exePath[len] = '\0';  // end string
                            std::string exeName = basename(exePath); // Get only executable name
                            if (exeName == e) {
                                // Kill the process
                                pid_t pid = atoi(ent->d_name);
                                kill(pid, SIGKILL);
                            }
                        }
                    }
                }
                closedir(dir);
            }
        }

    #endif
}