@echo off
g++ -Wconversion-null -c main.cpp -I C:\SFML\include 
g++ -Wconversion-null main.o -o main.exe -L C:\SFML\lib -lsfml-graphics -lsfml-window -lsfml-system -lws2_32