//
// Created by Steffen Schümann on 05.03.25.
//
#pragma once

#include <string>

struct UserDirectories {
    std::string home;
    std::string documents;
    std::string downloads;
};

extern UserDirectories getUserDirectories();
