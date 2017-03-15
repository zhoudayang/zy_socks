set(SOURCE_FILES
        local_server.cc
        send_thread_pool.cc
        tunnel.cc
        client_main.cc
        )

add_executable(local_server ${SOURCE_FILES})
