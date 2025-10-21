# wsrouter & wsclient

A lightweight brokered star topology Websocket architecture for IoT devices, written in C++. It facilitates communication between devices using a simple protocol.

## Concept

The system consists of two programs:

`wsrouter` is the broker server. It runs on a single dedicated network device and forwards Websocket messages.
`wsclient` is the client. It runs on every other device that sends or receives messages.

Instances of `wsclient` running on client devices send messages to `wsrouter`, which then forwards them to the recipient client device. This allows fast, efficient, low latency communication between nodes of an IoT network.

## Setup

###  Launching `wsrouter`

Designate one of your devices as the router. Ideally this is a server or a physical router.
It is recommended, although not mandatory, to start the router before any clients.
The router need not be run as root.

**Command line parameters:**

|Parameter|Meaning|
|---|---|
|`--port`, `-p`|Websocket port. Default: 8080.|
|`--connections`, `-c`|Maximum number of connections allowed. Default: 10.|
|`--log`, `-l`|Log all incoming and outgoing messages to the console.|
|`--verbose`|Allow `websocketpp` to print console messages. (Warning: it's really chatty!)|
|`--version`, `-v`|Show version number|
|`--help`, `-h`|Get help|

### Launching `wsclient`

Simply run `wsclient` on any device, and it will attempt to connect to the router. It will keep retrying until it succeeds or hits the retry limit.
It is recommended, but not mandatory, to run `wsclient` as a root process.

#### Command line parameters:

#### Connection
|Parameter|Arguments|Meaning|
|---|---|---|
|`--host`, `-h`|Host name|Hostname (IP) and port of the router. Default: `ws://192.168.8.1:8080`|
|`--id`, `-i`|Client ID|Name of this client. Default: `noname`|
|`--retries`, `-r`|Number of retries|Attempts to reconnect if Websocket connection is dropped. 0 means infinite. Default: `10`|
|`--retry_interval`, `-ri`|Milliseconds|Milliseconds to wait between reconnection attempts. Default: `1000`|
|`--timeout`, `-t`|Milliseconds|Timeout limit for reconnection attempts. Default: `2000`|

#### FIFO pipes
|Parameter|Arguments|Meaning|
|---|---|---|
|`--disable_pipe_all`, `-dp`||Disable forwarding every incoming message to the output FIFO pipe, except the PIPE command|
|`--pipe-in`, `-pi`|Pipe path|Input FIFO pipe. Default: `/tmp/ws_in`|
|`--pipe-out`, `-po`|Pipe path|Output FIFO pipe. Default: `/tmp/ws_out`|

### Others
|Parameter|Arguments|Meaning|
|---|---|---|
|`--help`, `-h`||Get help|
|`--log`, `-l`||Log all incoming and outgoing messages to the console.|
|`--pid`, `-p`|Path to PID file|Store the process ID in a file. Default: `/tmp/ws.pid`|
|`--version`, `-v`||Show version|

###  FIFO pipes

Each `wsclient` instance implements an input and an output FIFO channel. The program will attempt to create these upon startup. Default names are `/tmp/ws_in` and `/tmp/ws_out`. These pipes serve as an interface to connect your own programs to the client, so it can send and receive messages. The router does not have this feature.

A client may forward messages to `/tmp/ws_in`, using a special command, for other processes to receive them.

To send a message through a client instance, send it to `/tmp/ws_out`. It will be forwarded to the router as is. The client will not add anything. You must format your message, add the sender and recipient ID all the necessary flags and the payload.

Pipe paths can be set with a command line parameter. If needed, you can also create FIFO pipes manually: `mkfifo /tmp/my_fifo`.

##  The communications protocol

The system uses a simple, non-encrypted protocol. Messages are sent as simple strings. There is no length limit, except what your network (and common sense) may enforce.

### General format of messages between clients

```
<recipient id>::<sender id>::<reply expected>::<reply to>::<payload>
```

**Example:** `llm::frontend::1::frontend::Why is the sky blue?`

This example has 5 fields:

|Field|Meaning|
|---|---|
|`recipient_id`|Identifier of the client this message is being sent to|
|`sender_id`|Identifier of the sending client|
|`reply_expected`|A flag of `1` or `0`, indicating whether the sender is expecting a response. This is merely a convenience and not enforced in any way.|
|`reply_to`|The client to which the response should be sent to. This is useful to create an execution chain. It must not be `router`.|
|`payload`|The content of the message|

The `::` delimiter is ignored in the payload, except for some built-in commands.

The payload can be binary content, but it is recommended to encode it.

Streaming isn't directly supported, but you can still send data as encoded binary chunks. It's not very optimal but it might still work.

## Built-in commands

Both the router and the client can recognize and execute some simple commands. For clients, commands are sent in the standard message format described earlier. For the router, the format is slightly different.

## Commands to clients

The following commands are recognized by `wsclient`. Commands are not case sensitive.

#### `ping`
Tests whether the client is responding.

**Example**: `recipient::sender::1::::ping`
**Response:** `sender::recipient::0::::PONG`

#### `date` or `time`
Sets system date, time (optional) and timezone (optional) on the recipient device. Requires root privileges!

**Example:** `recipient::sender::1::::date::2025::09:15::13::37::00::Europe/Budapest`
**Response:** `sender::recipient::0::::New date/time set`

#### `shutdown`
Shuts down the client device. All running processes will receive a `SIGTERM` signal, then a `SIGKILL` after 30 seconds. Requires root privileges!

**Example:** `recipient::sender::0::::shutdown`

## Commands to the router

The router receives commands in a slightly different format:

```
router::<sender>::<command>::<arguments>
```

The router recognizes the following commands:

#### `hello`
Identifies a new client after connection. A client cannot receive any messages before "introducing itself". This command will be sent by `wsclient` automatically upon connection. You won't normally need to send it yourself. It is useful, however, when you're using a Websocket test application to simulate a client.

**Example:** `router::frontend::hello`
**Response:** `router::0::::hello frontend`

### `ping`
A ping. Same as for `wsclient`.

**Example:** `router::frontend::ping`

### `disconnect::<client id>`
Instructs the router to disconnect a certain client. An empty `client_id` will disconnect every unconfirmed client (those which never sent a `hello` or other command). Sending `*` will disconnect every client, confirmed or unconfirmed, including the sender.

This is an internal command. It is used by client instances when executing the `shutdown` command. Calling it directly wouldn't do anything because clients reconnect automatically.

### `clients::<client_id>::<arguments>`
Returns the client ID if the specified client is connected. If the client doesn't exist, the router returns an error message. If a `*` character is specified, the response is a list of all connected client IDs. If the ID parameter is omitted, the response will be the count of confirmed and unconfirmed clients.

**Example:** `router::frontend::clients::*`
**Response:** `router::0::::llm,frontend,dashcam` (The list of currently connected clients)

**Example:** `router::frontend::clients::dashcam`
**Response:** `router::0::::dashcam` (The `dashcam` client exists and it is connected)

**Example:** `router::frontend::clients`
**Response:** `router::0::::3,2` (3 confirmed, 2 unconfirmed)

### `version`
Returns the version number and build date of the router.

## Error messages

Error messages are responses to malformed commands. A client can send an error message to another client:

```
recipient::sender::0::error::<error code>::<message>
```

**Example:**
```
dashcam::frontend::0::error::1::Camera isn't connected
```

Router errors are always returned to the client sending the command:

```
router::<error code>::::<error message>
```

Messages received from the `router` never need a reply, the `reply to` flag is repurposed in router error messages as an error code. `0` means a system message (not an error), anything higher is an error code.

Possible error messages:

|Code|Message|Reason|
|---|---|---|
|0|N/A|System message, not an error|
|1|`Message could not be parsed`|You sent something garbled|
|2|`Message is incomplete`|The message received by the router had less than 5 fields|
|3|`Client "<recipient>" is not connected to server`|You're attempting to message a nonexistent client|
|4|`Invalid <sender|recipient|reply> ID: \"<id>\"`|A non-alphanumeric character was found in one of the client IDs (sender or recipient)|
|5|`<Sender|Recipient> not specified`|Either the sender or the recipient ID is missing|
|6|`The router cannot be marked as sender, or be replied to.`|You specified `router` as sender or reply-to ID|
|7|`Router is full`|Maximum number of connections reached, the client can't connect to the router|
|8|`Invalid command: "<command>"`|The command isn't recognized by the router|

### What will NOT cause an error:

- More than 5 fields in a message
- The payload contains the substring `::` (parsing the payload is the recipient's task)
- An unconfirmed client trying to send a message (before sending `hello`). Such messages will be forwarded and the unconfirmed client registered as confirmed. Thus, `hello` is optional if your new client immediately sends something upon connection.

# Building a new binary

A build script is provided for your convenience:
```
bash build.sh <x86|x64|freebsd_x64|arm|arm64|mips> <client|router>
```
Requires the header-only libraries ASIO and WebSocket++, and links against the standard C++17 libraries (`pthread`, `libstdc++`, `libm`, `glibc`). No Boost or external dependencies are needed.

*Important:* The project uses `asio`, imported as a Git submodule. Currently this dependency is pinned at version 1.18.0. Do not upgrade because `websocketpp` (v0.8.2) is not currently fully compatible with the latest version (v1.36.0) due to API changes. This repo will be updated when `websocketpp` is fixed.

# Some remarks

- Only `ws://` is supported, not `wss://`.
- This is a very simple router. It's not a good idea to use it in a security-critical or public-facing environment.
- Feel free to make a pull request if you want to improve this thing!