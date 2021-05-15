cmake_minimum_required(VERSION 3.19)
project(ComunicarPCs C)

set(CMAKE_C_STANDARD 11)

add_executable(client client/client.c messages.h client/ui.c client/ui.h client/unsentqueue.c client/unsentqueue.h
    client/recieverthread.c client/recieverthread.h client/senderthread.c client/senderthread.h client/globalvars.h
    client/unrecievedqueue.c client/unrecievedqueue.h)
add_executable(server server.c messages.h)

target_link_libraries(client pthread ncurses)
target_link_libraries(server pthread)