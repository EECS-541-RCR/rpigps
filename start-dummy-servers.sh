# Start both in the background.
# Immitates drone listening for commands.
./dummyserver udp 5556 > commands_to_drone &
# Immitates Android device listening for GPS updates.
./dummyserver tcp 5559 > gps_to_android &
