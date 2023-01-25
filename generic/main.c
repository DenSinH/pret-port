#include "log.h"

extern void AgbMain();


int main(int argc, char** argv) {
  log_info("Calling AgbMain");
  AgbMain();
}