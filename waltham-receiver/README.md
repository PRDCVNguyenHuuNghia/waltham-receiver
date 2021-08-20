# Waltham-receiver

waltham-receiver component is a receiver side implementation for using Waltham
protocol to obtain and process remote output received from remoting-pluging
instantianted/created output by the compositor.

This component is designed to be used for evaluating the functionalities of
waltham-transmitter plugin.

This component also acts as weston client application to display/handle various
requests from actual weston client at transmitter side.

### Architecture

````

				ECU 1                                                                     ECU 2
              +-----------------------------------------------------+                    +----------------------------------------------+
              |        +-----------------+                          |                    |                                              |
              |        | IVI-Application |                          |                    |               +-----------+-----------+      |
              |        +-----------------+                          |                    |               | Gstreamer |           |      |
              |                 ^                                   |    Buffer   -----------------------> (Decode)  |           |      |
              |        wayland  |                         +----------------------/       |               +-----------+           |      |
              |                 v                         |         |    (Ethernet)      |               |     Waltham-receiver  |      |
              |   +----+---------------------+            |         |        ---------------------------->                       |      |
              |   |    |  Transmitter Plugin |<-----------------------------/            |               +-----------------------+      |
              |   |    |                     |            |         |  Waltham-Protocol  |                             ^                |
              |   |    |---------------------|            |         |                    |                     wayland |                |
              |   |    |  (remoting-plugin)  |------------+         |                    |                             v                |
              |   |    |                     |                      |                    |                 +---------------------+      |
              |   |    +-+-------------------+                      |                    |                 |                     |      |
              |   |                          |                      |                    |                 |       compositor    |      |
              |   |         compositor       |                      |                    |                 |                     |      |
              |   +------+-------------------+                      |                    |                 +----------------+----+      |
              |          |                                          |                    |                                  |           |
              |          v                                          |                    |                                  v           |
              |   +------------+                                    |                    |                            +----------+      |
              |   |  Display   |                                    |                    |                            |  Display |      |
              |   |            |                                    |                    |                            |          |      |
              |   +------------+                                    |                    |                            +----------+      |
              +-----------------------------------------------------+                    +----------------------------------------------+

````

### Build Steps (these only apply if building locally)

1. Prerequisite before building

    weston, wayland, waltham and gstreamer should be built and available.

2. In waltham-receiver directory, create build directory

        $ meson -Dprefix=$PREFIX_PATH build/

3. Run ninja

        $ ninja -C build/

4. waltham-receiver binary should be availaible in build directory

### Connection Establishment

1. Connect two board over ethernet.

2. Assign IP to both the boards and check if the simple ping works.

	For example:if transmitter IP: 192.168.2.51 and Waltham-Receiver IP:
	192.168.2.52 then

    $ping 192.168.2.52 (you can also ping vice versa)

3. Make sure that IP address specified in the weston.ini under
   [transmitter-output] matches the Waltham-Receiver IP.

4. Make sure that IP address on the transmitter side match the Waltham-Receiver
   IP.

### Basic test steps with AGL

0. Under AGL platform you should already have the waltham-receiver installed.

1. Start the compositor with the transmitter plugin at the transmitter side,
   use agl-shell-app-id=app_id of the run the application and put it on
   transmitter screen. Setup a shared port between transmitter and receiver.
   Setup the receiver IP address. The transmitter-plugin uses the same section
   syntax as the remoting plugin.

2. Start the compositor at the receiver side

3. Start the receiver using -p <shared_port> -i <app_id>. If not app_id is
   specified the app_id passed by the transmitter will be used. Use -i <app_id>
   if you'd like to have the ability to activate and switch the surface if you
   intended to start multiple application and still be able to send input and
   display the received streamed buffers from the transmitter side.

4. Start the application on the transmitter side and watch it appear on the
   receiver side.
