#include "Terminal.h"
#include "ArchThreads.h"
#include "Console.h"

#include "KeyboardManager.h"

#include "kprintf.h"
#include "ustring.h"

Terminal::Terminal(char *name, Console *console, uint32 num_columns,
                   uint32 num_rows)
    : CharacterDevice(name), console_(console), num_columns_(num_columns),
      num_rows_(num_rows), len_(num_rows * num_columns), current_column_(0),
      current_state_(0x93), active_(0), mutex_("Terminal::mutex_"),
      layout_(EN) {
  characters_ = new uint8[len_];
  character_states_ = new uint8[len_];

  uint32 i;
  for (i = 0; i < len_; ++i) {
    characters_[i] = ' ';
    character_states_[i] = 0;
  }
}

void Terminal::clearBuffer() { in_buffer_.clear(); }

void Terminal::putInBuffer(uint32 what) { in_buffer_.put(what); }

char Terminal::read() { return (char)in_buffer_.get(); }

void Terminal::backspace(void) {
  if (in_buffer_.countElementsAhead())
    in_buffer_.get();

  if (current_column_) {
    --current_column_;
    setCharacter(num_rows_ - 1, current_column_, ' ');
  }
}

uint32 Terminal::readLine(char *line, uint32 size) {
  uint32 cchar;
  uint32 counter = 0;
  if (size < 1)
    return 0;
  do {
    cchar = in_buffer_.get();

    line[counter++] = (char)cchar;
  } while (cchar != '\n' && cchar != '\r' && counter < size);

  if (size - counter)
    line[counter] = '\0';
  return counter;
}

void Terminal::writeInternal(char character) {
  if (character == '\n' || character == '\r') {
    while (current_column_ < num_columns_) {
      setCharacter(num_rows_ - 1, current_column_++, ' ');
    }
    scrollUp();
    current_column_ = 0;
  } else if (character == '\b')
    backspace();
  else {
    setCharacter(num_rows_ - 1, current_column_, character);
    ++current_column_;
    if (current_column_ >= num_columns_) {
      // scroll up
      scrollUp();
      current_column_ = 0;
    }
  }
}

void Terminal::write(char character) {
  ScopeLock lock(mutex_);
  console_->lockConsoleForDrawing();
  writeInternal(character);
  console_->unLockConsoleForDrawing();
}
void Terminal::writeString(char const *string) {
  ScopeLock lock(mutex_);
  console_->lockConsoleForDrawing();
  while (string && *string) {
    writeInternal(*string);
    ++string;
  }
  console_->unLockConsoleForDrawing();
}

int32 Terminal::writeData(uint32 offset, uint32 size, const char *buffer) {
  if (offset != 0)
    return offset;

  writeBuffer(buffer, size);
  return size;
}

void Terminal::writeBuffer(char const *buffer, size_t len) {
  ScopeLock lock(mutex_);
  console_->lockConsoleForDrawing();
  while (len) {
    writeInternal(*buffer);
    ++buffer;
    --len;
  }
  console_->unLockConsoleForDrawing();
}

void Terminal::clearScreen() {
  uint32 i;
  for (i = 0; i < len_; ++i) {
    characters_[i] = ' ';
    character_states_[i] = current_state_;
  }
  console_->consoleClearScreen();
}

uint32 Terminal::getNumRows() const { return num_rows_; }

uint32 Terminal::getNumColumns() const { return num_columns_; }

uint32 Terminal::setCharacter(uint32 row, uint32 column, uint8 character) {
  characters_[column + row * num_columns_] = character;
  character_states_[column + row * num_columns_] = current_state_;
  if (active_)
    console_->consoleSetCharacter(row, column, character, current_state_);

  return 0;
}

void Terminal::setForegroundColor(Console::CONSOLECOLOR const &color) {
  ScopeLock lock(mutex_);
  // 4 bit set == 1+2+4+8, shifted by 0 bits
  uint8 mask = 15;
  current_state_ = current_state_ & ~mask;
  current_state_ |= color;
}

void Terminal::setBackgroundColor(Console::CONSOLECOLOR const &color) {
  ScopeLock lock(mutex_);
  // 4 bit set == 1+2+4+8, shifted by 4 bits
  uint8 mask = 15 << 4;
  uint8 col = color;
  current_state_ = current_state_ & ~mask;
  current_state_ |= col << 4;
}

void Terminal::initTerminalColors(Console::CONSOLECOLOR fg,
                                  Console::CONSOLECOLOR bg) {
  setForegroundColor(fg);
  setBackgroundColor(bg);
  clearScreen();
  fullRedraw();
}

void Terminal::scrollUp() {
  memmove(characters_, &characters_[num_columns_],
          (num_rows_ - 1) * num_columns_);
  memmove(character_states_, &character_states_[num_columns_],
          (num_rows_ - 1) * num_columns_);

  memset(&characters_[(num_rows_ - 1) * num_columns_], 0, num_columns_);
  memset(&character_states_[(num_rows_ - 1) * num_columns_], 0, num_columns_);

  if (active_)
    console_->consoleScrollUp(current_state_);
}
void Terminal::fullRedraw() {
  console_->lockConsoleForDrawing();
  uint32 i, k;
  uint32 runner = 0;
  for (i = 0; i < num_rows_; ++i) {
    for (k = 0; k < num_columns_; ++k) {
      console_->consoleSetCharacter(i, k, characters_[runner],
                                    character_states_[runner]);
      ++runner;
    }
  }

  console_->unLockConsoleForDrawing();
}

void Terminal::setAsActiveTerminal() {
  ScopeLock lock(mutex_);
  active_ = 1;
  fullRedraw();
}

void Terminal::unSetAsActiveTerminal() {
  ScopeLock lock(mutex_);
  active_ = 0;
}

void Terminal::print404(const char* error_message) {
  // set blue background
  uint8 mask = 15 << 4;
  uint8 col = Console::BLUE;
  current_state_ = current_state_ & ~mask;
  current_state_ |= col << 4;

  const char* msg = "Bro something went wrong :(";
  size_t msgLen = 0; while (msg[msgLen] != '\0') ++msgLen;
  size_t startCol = (num_columns_ > msgLen) ? (num_columns_ - msgLen) / 2 : 0;

  // get current thread information
  const char* thread_type =
      (currentThread && currentThread->type_ == Thread::USER_THREAD)
          ? "User Thread"
          : "Kernel Thread";
  size_t threadTypeLen = 0; while (thread_type[threadTypeLen] != '\0') ++threadTypeLen;

  size_t current_error_char = 0;

  for (size_t j = 0; j < num_rows_; j++) {
    for (size_t i = 0; i < num_columns_; i++) {
      if (j == num_rows_ / 2 && i >= startCol && i < startCol + msgLen) {
        writeInternal(msg[i - startCol]);
      } // print thread info
      else if (j == num_rows_ - 4) {
        size_t startThreadCol = (num_columns_ > threadTypeLen)
                                    ? (num_columns_ - threadTypeLen) / 2
                                    : 0;
        if (i >= startThreadCol && i < startThreadCol + threadTypeLen) {
          writeInternal(thread_type[i - startThreadCol]);
        } else {
          writeInternal('-');
        }
        // write the error message
      } else if (j > num_rows_ - 4) {
        size_t error_message_len = 0;
        if (error_message) { while (error_message[error_message_len] != '\0') ++error_message_len; }
        if (current_error_char < error_message_len) {
          writeInternal(error_message[current_error_char++]);
        } else {
          writeInternal(' ');
        }
      } else {
        writeInternal(' ');
      }
    }
  }
}
