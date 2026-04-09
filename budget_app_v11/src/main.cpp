#include "app.hpp"
#include "storage.hpp"
#include <filesystem>

int main() {
    budget::Storage storage(std::filesystem::path("data") / "budget_data.txt");
    budget::runApp(storage);
    return 0;
}
