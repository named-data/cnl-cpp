/**
 * Copyright (C) 2019 Regents of the University of California.
 * @author: Jeff Thompson <jefft0@remap.ucla.edu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version, with the additional exemption that
 * compiling, linking, and/or using OpenSSL is allowed.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * A copy of the GNU Lesser General Public License is in the file COPYING.
 */

/**
 * This registers with the local NFD to test sync functionality.
 */

#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <ndn-cpp/security/key-chain.hpp>
#include <cnl-cpp/namespace.hpp>

using namespace std;
using namespace ndn;
using namespace cnl_cpp;

static const char *WHITESPACE_CHARS = " \n\r\t";

/**
 * Modify str in place to erase whitespace on the left.
 * @param str
 */
static inline void
trimLeft(string& str)
{
  size_t found = str.find_first_not_of(WHITESPACE_CHARS);
  if (found != string::npos) {
    if (found > 0)
      str.erase(0, found);
  }
  else
    // All whitespace
    str.clear();
}

/**
 * Modify str in place to erase whitespace on the right.
 * @param str
 */
static inline void
trimRight(string& str)
{
  size_t found = str.find_last_not_of(WHITESPACE_CHARS);
  if (found != string::npos) {
    if (found + 1 < str.size())
      str.erase(found + 1);
  }
  else
    // All whitespace
    str.clear();
}

/**
 * Modify str in place to erase whitespace on the left and right.
 * @param str
 */
static void
trim(string& str)
{
  trimLeft(str);
  trimRight(str);
}

/**
 * Read a line from from stdin and return a trimmed string.  (We don't use
 * cin because it ignores a blank line.)
 */
static string
stdinReadLine()
{
  char inputBuffer[1000];
  ssize_t nBytes = ::read(STDIN_FILENO, inputBuffer, sizeof(inputBuffer) - 1);
  if (nBytes < 0)
    // Don't expect an error reading from stdin.
    throw runtime_error("stdinReadLine: error reading from STDIN_FILENO");

  inputBuffer[nBytes] = 0;
  string input(inputBuffer);
  trim(input);

  return input;
}

int main(int argc, char** argv)
{
  // Note: To turn off logging, in log4cxx.properties change rootLogger to FATAL.

  // Silence the warning from Interest wire encode.
  Interest::setDefaultCanBePrefix(true);

  cout << "Enter your user name (e.g. \"a\" or \"b\")" << endl;
  string userName = stdinReadLine();
  if (userName == "") {
    cout << "You must enter a user name" << endl;
    return 1;
  }

  try {
    // The default Face will connect using a Unix socket, or to "localhost".
    Face face;

    // Use the system default key chain and certificate name to sign.
    KeyChain keyChain;
    face.setCommandSigningInfo(keyChain, keyChain.getDefaultCertificateName());

    Namespace applicationPrefix(Name("/test/app"), &keyChain);
    applicationPrefix.setFace(&face, [](const ptr_lib::shared_ptr<const Name>& prefix) {
        cout << "Register failed for prefix " << prefix->toUri() << endl;
      });
    applicationPrefix.enableSync();

    Namespace& userPrefix = applicationPrefix[Name::Component(userName)];

    auto onStateChanged = [&]
      (Namespace& nameSpace, Namespace& changedNamespace, NamespaceState state,
       uint64_t callbackId) {
      if (state == NamespaceState_NAME_EXISTS &&
          !userPrefix.getName().isPrefixOf(changedNamespace.getName()))
        cout << "Received " << changedNamespace.getName().toUri() << endl;
    };
    applicationPrefix.addOnStateChanged(onStateChanged);

    Milliseconds publishIntervalMs = 1000.0;
    Name::Component component = Name("/%00").get(0);

    // Loop, producing a new name every publishIntervalMs milliseconds (and also
    // calling processEvents()).
    MillisecondsSince1970 previousPublishMs = 0;
    while (true) {
      MillisecondsSince1970 now = ndn_getNowMilliseconds();
      if (now >= previousPublishMs + publishIntervalMs) {
        // If userName is "a", this makes /test/app/a/%00, /test/app/a/%01, etc.
        Namespace& newNamespace = userPrefix[component];
        cout << "Publish " << newNamespace.getName().toUri() << endl;
        component = component.getSuccessor();

        previousPublishMs = now;
      }

      face.processEvents();
      // We need to sleep for a few milliseconds so we don't use 100% of the CPU.
      usleep(10000);
    }
  } catch (std::exception& e) {
    cout << "exception: " << e.what() << endl;
  }
  return 0;
}
