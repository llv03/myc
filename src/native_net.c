#include "native_net.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

Value native_net_listen(Value port_v) {
    int64_t port;
    if (!value_to_int(port_v, &port) || port < 0 || port > 65535)
        return nil_val();

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return nil_val();

    int yes = 1;
    (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return nil_val();
    }
    if (listen(fd, 16) < 0) {
        close(fd);
        return nil_val();
    }
    return int_val(fd);
}

Value native_net_accept(Value server_v) {
    int64_t server;
    if (!value_to_int(server_v, &server) || server < 0)
        return nil_val();

    struct sockaddr_in client;
    socklen_t len = sizeof(client);
    int cfd = accept((int)server, (struct sockaddr *)&client, &len);
    if (cfd < 0) return nil_val();
    return int_val(cfd);
}

Value native_net_recv(Value conn_v, Value maxn_v) {
    int64_t conn, maxn;
    if (!value_to_int(conn_v, &conn) || conn < 0)
        return obj_val((Obj *)obj_string_copy("", 0));
    if (!value_to_int(maxn_v, &maxn) || maxn <= 0)
        return obj_val((Obj *)obj_string_copy("", 0));
    if (maxn > 1024 * 1024) maxn = 1024 * 1024;

    char *buf = malloc((size_t)maxn + 1);
    if (!buf) return obj_val((Obj *)obj_string_copy("", 0));

    ssize_t n = recv((int)conn, buf, (size_t)maxn, 0);
    if (n < 0) {
        free(buf);
        return obj_val((Obj *)obj_string_copy("", 0));
    }
    buf[n] = '\0';
    return obj_val((Obj *)obj_string_take(buf, (int)n));
}

Value native_net_send(Value conn_v, Value s_v) {
    int64_t conn;
    if (!value_to_int(conn_v, &conn) || conn < 0)
        return nil_val();
    if (!is_string(s_v)) return nil_val();

    ObjString *s = as_string(s_v);
    const char *p = s->chars;
    int left = s->length;
    int total = 0;
    while (left > 0) {
        ssize_t n = send((int)conn, p, (size_t)left, 0);
        if (n < 0) {
            if (total == 0) return nil_val();
            return int_val(total);
        }
        p += n;
        left -= (int)n;
        total += (int)n;
    }
    return int_val(total);
}

Value native_net_close(Value id_v) {
    int64_t id;
    if (!value_to_int(id_v, &id) || id < 0)
        return nil_val();
    if (close((int)id) < 0) return nil_val();
    return bool_val(true);
}
