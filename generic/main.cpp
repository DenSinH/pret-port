#include "log.h"
#include "frontend.h"

extern "C" void AgbMain();


int main(int argc, char** argv) {
  log_info("Launching frontend");
  frontend::InitFrontend();
  log_info("Calling AgbMain");
  AgbMain();
}