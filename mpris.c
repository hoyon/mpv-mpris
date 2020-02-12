#include <gio/gio.h>
#include <glib-unix.h>
#include <mpv/client.h>
#include <inttypes.h>
#include <string.h>

static const char *introspection_xml =
    "<node>\n"
    "  <interface name=\"org.mpris.MediaPlayer2\">\n"
    "    <method name=\"Raise\">\n"
    "    </method>\n"
    "    <method name=\"Quit\">\n"
    "    </method>\n"
    "    <property name=\"CanQuit\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"Fullscreen\" type=\"b\" access=\"readwrite\"/>\n"
    "    <property name=\"CanSetFullscreen\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"CanRaise\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"HasTrackList\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"Identity\" type=\"s\" access=\"read\"/>\n"
    "    <property name=\"DesktopEntry\" type=\"s\" access=\"read\"/>\n"
    "    <property name=\"SupportedUriSchemes\" type=\"as\" access=\"read\"/>\n"
    "    <property name=\"SupportedMimeTypes\" type=\"as\" access=\"read\"/>\n"
    "  </interface>\n"
    "  <interface name=\"org.mpris.MediaPlayer2.Player\">\n"
    "    <method name=\"Next\">\n"
    "    </method>\n"
    "    <method name=\"Previous\">\n"
    "    </method>\n"
    "    <method name=\"Pause\">\n"
    "    </method>\n"
    "    <method name=\"PlayPause\">\n"
    "    </method>\n"
    "    <method name=\"Stop\">\n"
    "    </method>\n"
    "    <method name=\"Play\">\n"
    "    </method>\n"
    "    <method name=\"Seek\">\n"
    "      <arg type=\"x\" name=\"Offset\" direction=\"in\"/>\n"
    "    </method>\n"
    "    <method name=\"SetPosition\">\n"
    "      <arg type=\"o\" name=\"TrackId\" direction=\"in\"/>\n"
    "      <arg type=\"x\" name=\"Offset\" direction=\"in\"/>\n"
    "    </method>\n"
    "    <method name=\"OpenUri\">\n"
    "      <arg type=\"s\" name=\"Uri\" direction=\"in\"/>\n"
    "    </method>\n"
    "    <signal name=\"Seeked\">\n"
    "      <arg type=\"x\" name=\"Position\" direction=\"out\"/>\n"
    "    </signal>\n"
    "    <property name=\"PlaybackStatus\" type=\"s\" access=\"read\"/>\n"
    "    <property name=\"LoopStatus\" type=\"s\" access=\"readwrite\"/>\n"
    "    <property name=\"Rate\" type=\"d\" access=\"readwrite\"/>\n"
    "    <property name=\"Shuffle\" type=\"b\" access=\"readwrite\"/>\n"
    "    <property name=\"Metadata\" type=\"a{sv}\" access=\"read\"/>\n"
    "    <property name=\"Volume\" type=\"d\" access=\"readwrite\"/>\n"
    "    <property name=\"Position\" type=\"x\" access=\"read\"/>\n"
    "    <property name=\"MinimumRate\" type=\"d\" access=\"read\"/>\n"
    "    <property name=\"MaximumRate\" type=\"d\" access=\"read\"/>\n"
    "    <property name=\"CanGoNext\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"CanGoPrevious\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"CanPlay\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"CanPause\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"CanSeek\" type=\"b\" access=\"read\"/>\n"
    "    <property name=\"CanControl\" type=\"b\" access=\"read\"/>\n"
    "  </interface>\n"
    "</node>\n";

typedef struct UserData
{
    mpv_handle *mpv;
    GMainLoop *loop;
    gint bus_id;
    GDBusConnection *connection;
    GDBusInterfaceInfo *root_interface_info;
    GDBusInterfaceInfo *player_interface_info;
    guint root_interface_id;
    guint player_interface_id;
    const char *status;
    const char *loop_status;
    GHashTable *changed_properties;
    GVariant *metadata;
    gboolean seek_expected;
} UserData;

static const char *STATUS_PLAYING = "Playing";
static const char *STATUS_PAUSED = "Paused";
static const char *STATUS_STOPPED = "Stopped";
static const char *LOOP_NONE = "None";
static const char *LOOP_TRACK = "Track";
static const char *LOOP_PLAYLIST = "Playlist";

static gchar *string_to_utf8(gchar *maybe_utf8)
{
    gchar *attempted_validation;
#if GLIB_CHECK_VERSION(2, 52, 0)
    attempted_validation = g_utf8_make_valid(maybe_utf8, -1);
#else
    attempted_validation = g_strdup(maybe_utf8);
#endif

    if (g_utf8_validate(attempted_validation, -1, NULL)) {
        return attempted_validation;
    } else {
        g_free(attempted_validation);
        return g_strdup("<invalid utf8>");
    }
}

static void add_metadata_item_string(mpv_handle *mpv, GVariantDict *dict,
                                     const char *property, const char *tag)
{
    char *temp = mpv_get_property_string(mpv, property);
    if (temp) {
        char *utf8 = string_to_utf8(temp);
        g_variant_dict_insert(dict, tag, "s", utf8);
        g_free(utf8);
        mpv_free(temp);
    }
}

static void add_metadata_item_int(mpv_handle *mpv, GVariantDict *dict,
                                  const char *property, const char *tag)
{
    int64_t value;
    int res = mpv_get_property(mpv, property, MPV_FORMAT_INT64, &value);
    if (res >= 0) {
        g_variant_dict_insert(dict, tag, "x", value);
    }
}

static void add_metadata_item_string_list(mpv_handle *mpv, GVariantDict *dict,
                                          const char *property, const char *tag)
{
    char *temp = mpv_get_property_string(mpv, property);

    if (temp) {
        GVariantBuilder builder;
        char **list = g_strsplit(temp, ", ", 0);
        char **iter = list;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));

        for (; *iter; iter++) {
            char *item = *iter;
            char *utf8 = string_to_utf8(item);
            g_variant_builder_add(&builder, "s", utf8);
            g_free(utf8);
        }

        g_variant_dict_insert(dict, tag, "as", &builder);

        g_strfreev(list);
        mpv_free(temp);
    }
}

static gchar *path_to_uri(mpv_handle *mpv, char *path)
{
#if GLIB_CHECK_VERSION(2, 58, 0)
    // version which uses g_canonicalize_filename which expands .. and .
    // and makes the uris neater
    char* working_dir;
    gchar* canonical;
    gchar *uri;

    working_dir = mpv_get_property_string(mpv, "working-directory");
    canonical = g_canonicalize_filename(path, working_dir);
    uri = g_filename_to_uri(canonical, NULL, NULL);

    mpv_free(working_dir);
    g_free(canonical);

    return uri;
#else
    // for compatibility with older versions of glib
    gchar *converted;
    if (g_path_is_absolute(path)) {
        converted = g_filename_to_uri(path, NULL, NULL);
    } else {
        char* working_dir;
        gchar* absolute;

        working_dir = mpv_get_property_string(mpv, "working-directory");
        absolute = g_build_filename(working_dir, path, NULL);
        converted = g_filename_to_uri(absolute, NULL, NULL);

        mpv_free(working_dir);
        g_free(absolute);
    }

    return converted;
#endif
}

static void add_metadata_uri(mpv_handle *mpv, GVariantDict *dict)
{
    char *path;
    char *uri;

    path = mpv_get_property_string(mpv, "path");
    if (!path) {
        return;
    }

    uri = g_uri_parse_scheme(path);
    if (uri) {
        g_variant_dict_insert(dict, "xesam:url", "s", path);
        g_free(uri);
    } else {
        gchar *converted = path_to_uri(mpv, path);
        g_variant_dict_insert(dict, "xesam:url", "s", converted);
        g_free(converted);
    }

    mpv_free(path);
}

// Copied from https://github.com/videolan/vlc/blob/master/modules/meta_engine/folder.c
static const char art_files[][20] = {
    "Folder.jpg",           /* Windows */
    "Folder.png",
    "AlbumArtSmall.jpg",    /* Windows */
    "AlbumArt.jpg",         /* Windows */
    "Album.jpg",
    ".folder.png",          /* KDE?    */
    "cover.jpg",            /* rockbox */
    "cover.png",
    "cover.gif",
    "front.jpg",
    "front.png",
    "front.gif",
    "front.bmp",
    "thumb.jpg",
};

static const int art_files_count = sizeof(art_files) / sizeof(art_files[0]);

static void add_metadata_art(mpv_handle *mpv, GVariantDict *dict)
{
    char *path;
    gchar *dirname;
    gboolean found = FALSE;

    path = mpv_get_property_string(mpv, "path");

    if (!path) {
        return;
    }

    dirname = g_path_get_dirname(path);

    for (int i = 0; i < art_files_count; i++) {
        gchar *filename = g_build_filename(dirname, art_files[i], NULL);

        if (g_file_test(filename, G_FILE_TEST_EXISTS)) {
            gchar *uri = path_to_uri(mpv, filename);
            g_variant_dict_insert(dict, "mpris:artUrl", "s", uri);
            g_free(uri);
            found = TRUE;
        }

        g_free(filename);

        if (found) {
            break;
        }
    }

    g_free(dirname);
    mpv_free(path);
}

static void add_metadata_content_created(mpv_handle *mpv, GVariantDict *dict)
{
    char *date = mpv_get_property_string(mpv, "metadata/by-key/Date");

    if (!date) {
        return;
    }

    // Bail if date is not just the year
    if (strlen(date) == 4) {
        gchar *iso8601 = g_strdup_printf("%s-01-01T00:00:00Z", date);
        g_variant_dict_insert(dict, "xesam:contentCreated", "s", iso8601);
        g_free(iso8601);
    }

    mpv_free(date);
}

static GVariant *create_metadata(UserData *ud)
{
    GVariantDict dict;
    int64_t track;
    double duration;
    char *temp_str;
    int res;

    g_variant_dict_init(&dict, NULL);

    // mpris:trackid
    mpv_get_property(ud->mpv, "playlist-pos", MPV_FORMAT_INT64, &track);
    temp_str = g_strdup_printf("/%" PRId64, track);
    g_variant_dict_insert(&dict, "mpris:trackid", "o", temp_str);
    g_free(temp_str);

    // mpris:length
    res = mpv_get_property(ud->mpv, "duration", MPV_FORMAT_DOUBLE, &duration);
    if (res == MPV_ERROR_SUCCESS) {
        g_variant_dict_insert(&dict, "mpris:length", "x", (int64_t)(duration * 1000000.0));
    }

    // initial value. Replaced with metadata value if available
    add_metadata_item_string(ud->mpv, &dict, "media-title", "xesam:title");

    add_metadata_item_string(ud->mpv, &dict, "metadata/by-key/Title", "xesam:title");
    add_metadata_item_string(ud->mpv, &dict, "metadata/by-key/Album", "xesam:album");
    add_metadata_item_string(ud->mpv, &dict, "metadata/by-key/Genre", "xesam:genre");

    add_metadata_item_string_list(ud->mpv, &dict, "metadata/by-key/Artist", "xesam:artist");
    add_metadata_item_string_list(ud->mpv, &dict, "metadata/by-key/Album_Artist", "xesam:albumArtist");
    add_metadata_item_string_list(ud->mpv, &dict, "metadata/by-key/Composer", "xesam:composer");

    add_metadata_item_int(ud->mpv, &dict, "metadata/by-key/Track", "xesam:trackNumber");
    add_metadata_item_int(ud->mpv, &dict, "metadata/by-key/Disc", "xesam:discNumber");

    add_metadata_uri(ud->mpv, &dict);
    add_metadata_art(ud->mpv, &dict);
    add_metadata_content_created(ud->mpv, &dict);

    return g_variant_dict_end(&dict);
}

static void method_call_root(G_GNUC_UNUSED GDBusConnection *connection,
                             G_GNUC_UNUSED const char *sender,
                             G_GNUC_UNUSED const char *object_path,
                             G_GNUC_UNUSED const char *interface_name,
                             const char *method_name,
                             G_GNUC_UNUSED GVariant *parameters,
                             GDBusMethodInvocation *invocation,
                             gpointer user_data)
{
    UserData *ud = (UserData*)user_data;
    if (g_strcmp0(method_name, "Quit") == 0) {
        const char *cmd[] = {"quit", NULL};
        mpv_command_async(ud->mpv, 0, cmd);
        g_dbus_method_invocation_return_value(invocation, NULL);

    } else if (g_strcmp0(method_name, "Raise") == 0) {
        // Can't raise
        g_dbus_method_invocation_return_value(invocation, NULL);

    } else {
        g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
                                              G_DBUS_ERROR_UNKNOWN_METHOD,
                                              "Unknown method");
    }
}

static GVariant *get_property_root(G_GNUC_UNUSED GDBusConnection *connection,
                                   G_GNUC_UNUSED const char *sender,
                                   G_GNUC_UNUSED const char *object_path,
                                   G_GNUC_UNUSED const char *interface_name,
                                   const char *property_name,
                                   G_GNUC_UNUSED GError **error,
                                   gpointer user_data)
{
    UserData *ud = (UserData*)user_data;
    GVariant *ret;

    if (g_strcmp0(property_name, "CanQuit") == 0) {
        ret = g_variant_new_boolean(TRUE);

    } else if (g_strcmp0(property_name, "Fullscreen") == 0) {
        int fullscreen;
        mpv_get_property(ud->mpv, "fullscreen", MPV_FORMAT_FLAG, &fullscreen);
        ret = g_variant_new_boolean(fullscreen);

    } else if (g_strcmp0(property_name, "CanSetFullscreen") == 0) {
        int can_fullscreen;
        mpv_get_property(ud->mpv, "vo-configured", MPV_FORMAT_FLAG, &can_fullscreen);
        ret = g_variant_new_boolean(can_fullscreen);

    } else if (g_strcmp0(property_name, "CanRaise") == 0) {
        ret = g_variant_new_boolean(FALSE);

    } else if (g_strcmp0(property_name, "HasTrackList") == 0) {
        ret = g_variant_new_boolean(FALSE);

    } else if (g_strcmp0(property_name, "Identity") == 0) {
        ret = g_variant_new_string("mpv");

    } else if (g_strcmp0(property_name, "DesktopEntry") == 0) {
        ret = g_variant_new_string("mpv");

    } else if (g_strcmp0(property_name, "SupportedUriSchemes") == 0) {
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));
        g_variant_builder_add(&builder, "s", "ftp");
        g_variant_builder_add(&builder, "s", "http");
        g_variant_builder_add(&builder, "s", "https");
        g_variant_builder_add(&builder, "s", "mms");
        g_variant_builder_add(&builder, "s", "rtmp");
        g_variant_builder_add(&builder, "s", "rtsp");
        g_variant_builder_add(&builder, "s", "sftp");
        g_variant_builder_add(&builder, "s", "smb");
        ret = g_variant_builder_end(&builder);

    } else if (g_strcmp0(property_name, "SupportedMimeTypes") == 0) {
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));
        g_variant_builder_add(&builder, "s", "application/ogg");
        g_variant_builder_add(&builder, "s", "audio/mpeg");
        // TODO add the rest
        ret = g_variant_builder_end(&builder);

    } else {
        ret = NULL;
        g_set_error(error, G_DBUS_ERROR,
                    G_DBUS_ERROR_UNKNOWN_PROPERTY,
                    "Unknown property %s", property_name);
    }

    return ret;
}

static gboolean set_property_root(G_GNUC_UNUSED GDBusConnection *connection,
                                  G_GNUC_UNUSED const char *sender,
                                  G_GNUC_UNUSED const char *object_path,
                                  G_GNUC_UNUSED const char *interface_name,
                                  const char *property_name,
                                  GVariant *value,
                                  G_GNUC_UNUSED GError **error,
                                  gpointer user_data)
{
    UserData *ud = (UserData*)user_data;
    if (g_strcmp0(property_name, "Fullscreen") == 0) {
        int fullscreen;
        g_variant_get(value, "b", &fullscreen);
        mpv_set_property(ud->mpv, "fullscreen", MPV_FORMAT_FLAG, &fullscreen);

    } else {
        g_set_error(error, G_DBUS_ERROR,
                    G_DBUS_ERROR_UNKNOWN_PROPERTY,
                    "Cannot set property %s", property_name);
        return FALSE;
    }
    return TRUE;
}

static GDBusInterfaceVTable vtable_root = {
    method_call_root, get_property_root, set_property_root, {0}
};

static void method_call_player(G_GNUC_UNUSED GDBusConnection *connection,
                               G_GNUC_UNUSED const char *sender,
                               G_GNUC_UNUSED const char *object_path,
                               G_GNUC_UNUSED const char *interface_name,
                               const char *method_name,
                               G_GNUC_UNUSED GVariant *parameters,
                               GDBusMethodInvocation *invocation,
                               gpointer user_data)
{
    UserData *ud = (UserData*)user_data;
    if (g_strcmp0(method_name, "Pause") == 0) {
        int paused = TRUE;
        mpv_set_property(ud->mpv, "pause", MPV_FORMAT_FLAG, &paused);
        g_dbus_method_invocation_return_value(invocation, NULL);

    } else if (g_strcmp0(method_name, "PlayPause") == 0) {
        int paused;
        if (ud->status == STATUS_PAUSED) {
            paused = FALSE;
        } else {
            paused = TRUE;
        }
        mpv_set_property(ud->mpv, "pause", MPV_FORMAT_FLAG, &paused);
        g_dbus_method_invocation_return_value(invocation, NULL);

    } else if (g_strcmp0(method_name, "Play") == 0) {
        int paused = FALSE;
        mpv_set_property(ud->mpv, "pause", MPV_FORMAT_FLAG, &paused);
        g_dbus_method_invocation_return_value(invocation, NULL);

    } else if (g_strcmp0(method_name, "Stop") == 0) {
        const char *cmd[] = {"stop", NULL};
        mpv_command_async(ud->mpv, 0, cmd);
        g_dbus_method_invocation_return_value(invocation, NULL);

    } else if (g_strcmp0(method_name, "Next") == 0) {
        const char *cmd[] = {"playlist_next", NULL};
        mpv_command_async(ud->mpv, 0, cmd);
        g_dbus_method_invocation_return_value(invocation, NULL);

    } else if (g_strcmp0(method_name, "Previous") == 0) {
        const char *cmd[] = {"playlist_prev", NULL};
        mpv_command_async(ud->mpv, 0, cmd);
        g_dbus_method_invocation_return_value(invocation, NULL);

    } else if (g_strcmp0(method_name, "Seek") == 0) {
        int64_t offset_us; // in microseconds
        char *offset_str;
        g_variant_get(parameters, "(x)", &offset_us);
        double offset_s = offset_us / 1000000.0;
        offset_str = g_strdup_printf("%f", offset_s);

        const char *cmd[] = {"seek", offset_str, NULL};
        mpv_command_async(ud->mpv, 0, cmd);
        g_dbus_method_invocation_return_value(invocation, NULL);
        g_free(offset_str);

    } else if (g_strcmp0(method_name, "SetPosition") == 0) {
        int64_t current_id;
        char *object_path;
        double new_position_s;
        int64_t new_position_us;

        mpv_get_property(ud->mpv, "playlist-pos", MPV_FORMAT_INT64, &current_id);
        g_variant_get(parameters, "(&ox)", &object_path, &new_position_us);
        new_position_s = ((float)new_position_us) / 1000000.0; // us -> s

        if (current_id == g_ascii_strtoll(object_path + 1, NULL, 10)) {
            mpv_set_property(ud->mpv, "time-pos", MPV_FORMAT_DOUBLE, &new_position_s);
        }

        g_dbus_method_invocation_return_value(invocation, NULL);

    } else if (g_strcmp0(method_name, "OpenUri") == 0) {
        char *uri;
        g_variant_get(parameters, "(&s)", &uri);
        const char *cmd[] = {"loadfile", uri, NULL};
        mpv_command_async(ud->mpv, 0, cmd);
        g_dbus_method_invocation_return_value(invocation, NULL);

    } else {
        g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
                                              G_DBUS_ERROR_UNKNOWN_METHOD,
                                              "Unknown method");
    }
}

static GVariant *get_property_player(G_GNUC_UNUSED GDBusConnection *connection,
                                     G_GNUC_UNUSED const char *sender,
                                     G_GNUC_UNUSED const char *object_path,
                                     G_GNUC_UNUSED const char *interface_name,
                                     const char *property_name,
                                     GError **error,
                                     gpointer user_data)
{
    UserData *ud = (UserData*)user_data;
    GVariant *ret;
    if (g_strcmp0(property_name, "PlaybackStatus") == 0) {
        ret = g_variant_new_string(ud->status);

    } else if (g_strcmp0(property_name, "LoopStatus") == 0) {
        ret = g_variant_new_string(ud->loop_status);

    } else if (g_strcmp0(property_name, "Rate") == 0) {
        double rate;
        mpv_get_property(ud->mpv, "speed", MPV_FORMAT_DOUBLE, &rate);
        ret = g_variant_new_double(rate);

    } else if (g_strcmp0(property_name, "Shuffle") == 0) {
        int shuffle;
        mpv_get_property(ud->mpv, "playlist-shuffle", MPV_FORMAT_FLAG, &shuffle);
        ret = g_variant_new_boolean(shuffle);

    } else if (g_strcmp0(property_name, "Metadata") == 0) {
        if (!ud->metadata) {
            ud->metadata = create_metadata(ud);
        }
        // Increase reference count to prevent it from being freed after returning
        g_variant_ref(ud->metadata);
        ret = ud->metadata;

    } else if (g_strcmp0(property_name, "Volume") == 0) {
        double volume;
        mpv_get_property(ud->mpv, "volume", MPV_FORMAT_DOUBLE, &volume);
        volume /= 100;
        ret = g_variant_new_double(volume);

    } else if (g_strcmp0(property_name, "Position") == 0) {
        double position_s;
        int64_t position_us;
        mpv_get_property(ud->mpv, "time-pos", MPV_FORMAT_DOUBLE, &position_s);
        position_us = position_s * 1000000.0; // s -> us
        ret = g_variant_new_int64(position_us);

    } else if (g_strcmp0(property_name, "MinimumRate") == 0) {
        ret = g_variant_new_double(0.01);

    } else if (g_strcmp0(property_name, "MaximumRate") == 0) {
        ret = g_variant_new_double(100);

    } else if (g_strcmp0(property_name, "CanGoNext") == 0) {
        ret = g_variant_new_boolean(TRUE);

    } else if (g_strcmp0(property_name, "CanGoPrevious") == 0) {
        ret = g_variant_new_boolean(TRUE);

    } else if (g_strcmp0(property_name, "CanPlay") == 0) {
        ret = g_variant_new_boolean(TRUE);

    } else if (g_strcmp0(property_name, "CanPause") == 0) {
        ret = g_variant_new_boolean(TRUE);

    } else if (g_strcmp0(property_name, "CanSeek") == 0) {
        ret = g_variant_new_boolean(TRUE);

    } else if (g_strcmp0(property_name, "CanControl") == 0) {
        ret = g_variant_new_boolean(TRUE);

    } else {
        ret = NULL;
        g_set_error(error, G_DBUS_ERROR,
                    G_DBUS_ERROR_UNKNOWN_PROPERTY,
                    "Unknown property %s", property_name);
    }

    return ret;
}

static gboolean set_property_player(G_GNUC_UNUSED GDBusConnection *connection,
                                    G_GNUC_UNUSED const char *sender,
                                    G_GNUC_UNUSED const char *object_path,
                                    G_GNUC_UNUSED const char *interface_name,
                                    const char *property_name,
                                    GVariant *value,
                                    G_GNUC_UNUSED GError **error,
                                    gpointer user_data)
{
    UserData *ud = (UserData*)user_data;
    if (g_strcmp0(property_name, "LoopStatus") == 0) {
        const char *status;
        int t = TRUE;
        int f = FALSE;
        status = g_variant_get_string(value, NULL);
        if (g_strcmp0(status, "Track") == 0) {
            mpv_set_property(ud->mpv, "loop-file", MPV_FORMAT_FLAG, &t);
            mpv_set_property(ud->mpv, "loop-playlist", MPV_FORMAT_FLAG, &f);
        } else if (g_strcmp0(status, "Playlist") == 0) {
            mpv_set_property(ud->mpv, "loop-file", MPV_FORMAT_FLAG, &f);
            mpv_set_property(ud->mpv, "loop-playlist", MPV_FORMAT_FLAG, &t);
        } else {
            mpv_set_property(ud->mpv, "loop-file", MPV_FORMAT_FLAG, &f);
            mpv_set_property(ud->mpv, "loop-playlist", MPV_FORMAT_FLAG, &f);
        }

    } else if (g_strcmp0(property_name, "Rate") == 0) {
        double rate = g_variant_get_double(value);
        mpv_set_property(ud->mpv, "speed", MPV_FORMAT_DOUBLE, &rate);

    } else if (g_strcmp0(property_name, "Shuffle") == 0) {
        int shuffle = g_variant_get_boolean(value);
        mpv_set_property(ud->mpv, "playlist-shuffle", MPV_FORMAT_FLAG, &shuffle);

    } else if (g_strcmp0(property_name, "Volume") == 0) {
        double volume = g_variant_get_double(value);
        volume *= 100;
        mpv_set_property(ud->mpv, "volume", MPV_FORMAT_DOUBLE, &volume);

    } else {
        g_set_error(error, G_DBUS_ERROR,
                    G_DBUS_ERROR_UNKNOWN_PROPERTY,
                    "Cannot set property %s", property_name);
        return FALSE;
    }

    return TRUE;
}

static GDBusInterfaceVTable vtable_player = {
    method_call_player, get_property_player, set_property_player, {0}
};

static gboolean emit_property_changes(gpointer data)
{
    UserData *ud = (UserData*)data;
    GError *error = NULL;
    GVariant *params;
    GVariantBuilder *properties;
    GVariantBuilder *invalidated;
    gpointer prop_name, prop_value;
    GHashTableIter iter;

    if (g_hash_table_size(ud->changed_properties) > 0) {
        properties = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
        invalidated = g_variant_builder_new(G_VARIANT_TYPE("as"));
        g_hash_table_iter_init(&iter, ud->changed_properties);
        while (g_hash_table_iter_next(&iter, &prop_name, &prop_value)) {
            if (prop_value) {
                g_variant_builder_add(properties, "{sv}", prop_name, prop_value);
            } else {
                g_variant_builder_add(invalidated, "s", prop_name);
            }
        }
        params = g_variant_new("(sa{sv}as)",
                               "org.mpris.MediaPlayer2.Player", properties, invalidated);
        g_variant_builder_unref(properties);
        g_variant_builder_unref(invalidated);

        g_dbus_connection_emit_signal(ud->connection, NULL,
                                      "/org/mpris/MediaPlayer2",
                                      "org.freedesktop.DBus.Properties",
                                      "PropertiesChanged",
                                      params, &error);
        if (error != NULL) {
            g_printerr("%s", error->message);
        }

        g_hash_table_remove_all(ud->changed_properties);
    }
    return TRUE;
}

static void emit_seeked_signal(UserData *ud)
{
    GVariant *params;
    double position_s;
    int64_t position_us;
    GError *error = NULL;
    mpv_get_property(ud->mpv, "time-pos", MPV_FORMAT_DOUBLE, &position_s);
    position_us = position_s * 1000000.0; // s -> us
    params = g_variant_new("(x)", position_us);

    g_dbus_connection_emit_signal(ud->connection, NULL,
                                  "/org/mpris/MediaPlayer2",
                                  "org.mpris.MediaPlayer2.Player",
                                  "Seeked",
                                  params, &error);

    if (error != NULL) {
        g_printerr("%s", error->message);
    }
}

// Register dbus object and interfaces
static void on_bus_acquired(GDBusConnection *connection,
                            G_GNUC_UNUSED const char *name,
                            gpointer user_data)
{
    GError *error = NULL;
    UserData *ud = user_data;
    ud->connection = connection;

    ud->root_interface_id =
        g_dbus_connection_register_object(connection, "/org/mpris/MediaPlayer2",
                                          ud->root_interface_info,
                                          &vtable_root,
                                          user_data, NULL, &error);
    if (error != NULL) {
        g_printerr("%s", error->message);
    }

    ud->player_interface_id =
        g_dbus_connection_register_object(connection, "/org/mpris/MediaPlayer2",
                                        ud->player_interface_info,
                                        &vtable_player,
                                        user_data, NULL, &error);
    if (error != NULL) {
        g_printerr("%s", error->message);
    }
}

static void on_name_lost(GDBusConnection *connection,
                         G_GNUC_UNUSED const char *name,
                         gpointer user_data)
{
    if (connection) {
        UserData *ud = user_data;
        pid_t pid = getpid();
        char *name = g_strdup_printf("org.mpris.MediaPlayer2.mpv.instance%d", pid);
        ud->bus_id = g_bus_own_name(G_BUS_TYPE_SESSION,
                                    name,
                                    G_BUS_NAME_OWNER_FLAGS_NONE,
                                    NULL, NULL, NULL,
                                    &ud, NULL);
        g_free(name);
    }
}

static void handle_property_change(const char *name, void *data, UserData *ud)
{
    const char *prop_name = NULL;
    GVariant *prop_value = NULL;
    if (g_strcmp0(name, "pause") == 0) {
        int *paused = data;
        if (*paused) {
            ud->status = STATUS_PAUSED;
        } else {
            ud->status = STATUS_PLAYING;
        }
        prop_name = "PlaybackStatus";
        prop_value = g_variant_new_string(ud->status);

    } else if (g_strcmp0(name, "media-title") == 0 ||
               g_strcmp0(name, "duration") == 0) {
        // Free exising metadata object
        if (ud->metadata) {
            g_variant_unref(ud->metadata);
        }
        ud->metadata = create_metadata(ud);
        prop_name = "Metadata";
        prop_value = ud->metadata;

    } else if (g_strcmp0(name, "speed") == 0) {
        double *rate = data;
        prop_name = "Rate";
        prop_value = g_variant_new_double(*rate);

    } else if (g_strcmp0(name, "volume") == 0) {
        double *volume = data;
        *volume /= 100;
        prop_name = "Volume";
        prop_value = g_variant_new_double(*volume);

    } else if (g_strcmp0(name, "loop-file") == 0) {
        char *status = *(char **)data;
        if (g_strcmp0(status, "yes") == 0) {
            ud->loop_status = LOOP_TRACK;
        } else {
            char *playlist_status;
            mpv_get_property(ud->mpv, "loop-playlist", MPV_FORMAT_STRING, &playlist_status);
            if (g_strcmp0(playlist_status, "inf") == 0) {
                ud->loop_status = LOOP_PLAYLIST;
            } else {
                ud->loop_status = LOOP_NONE;
            }
        }

    } else if (g_strcmp0(name, "loop-playlist") == 0) {
        char *status = *(char **)data;
        if (g_strcmp0(status, "inf") == 0) {
            ud->loop_status = LOOP_PLAYLIST;
        } else {
            char *file_status;
            mpv_get_property(ud->mpv, "loop-file", MPV_FORMAT_STRING, &file_status);
            if (g_strcmp0(file_status, "yes") == 0) {
                ud->loop_status = LOOP_TRACK;
            } else {
                ud->loop_status = LOOP_NONE;
            }
        }
    }

    if (prop_name) {
        if (prop_value) {
            g_variant_ref(prop_value);
        }
        g_hash_table_insert(ud->changed_properties,
                            (gpointer)prop_name, prop_value);
    }
}

static gboolean event_handler(int fd, G_GNUC_UNUSED GIOCondition condition, gpointer data)
{
    UserData *ud = data;
    mpv_event *event;
    gboolean has_event = TRUE;

    // Discard data in pipe
    char unused[16];
    while (read(fd, unused, sizeof(unused)) > 0);

    while (has_event) {
        event = mpv_wait_event(ud->mpv, 0);
        switch (event->event_id) {
        case MPV_EVENT_NONE:
            has_event = FALSE;
            break;
        case MPV_EVENT_SHUTDOWN:
            g_main_loop_quit(ud->loop);
            break;
        case MPV_EVENT_IDLE:
            ud->status = STATUS_STOPPED;
            break;
        case MPV_EVENT_PROPERTY_CHANGE: {
            mpv_event_property *prop_event = (mpv_event_property*)event->data;
            handle_property_change(prop_event->name, prop_event->data, ud);
        } break;
        case MPV_EVENT_SEEK:
            ud->seek_expected = TRUE;
            break;
        case MPV_EVENT_PLAYBACK_RESTART: {
            if (ud->seek_expected) {
                emit_seeked_signal(ud);
                ud->seek_expected = FALSE;
            }
         } break;
        default:
            break;
        }
    }

    return TRUE;
}

static void wakeup_handler(void *fd)
{
    write(*((int*)fd), "0", 1);
}

// Plugin entry point
int mpv_open_cplugin(mpv_handle *mpv)
{
    GMainLoop *loop;
    UserData ud = {};
    GError *error = NULL;
    GDBusNodeInfo *introspection_data = NULL;
    int pipe[2];
    guint mpv_pipe_source;
    guint timeout_source;

    loop = g_main_loop_new(NULL, FALSE);

    // Load introspection data and split into separate interfaces
    introspection_data = g_dbus_node_info_new_for_xml(introspection_xml, &error);
    if (error != NULL) {
        g_printerr("%s", error->message);
    }
    ud.root_interface_info =
        g_dbus_node_info_lookup_interface(introspection_data, "org.mpris.MediaPlayer2");
    ud.player_interface_info =
        g_dbus_node_info_lookup_interface(introspection_data, "org.mpris.MediaPlayer2.Player");

    ud.mpv = mpv;
    ud.loop = loop;
    ud.status = STATUS_STOPPED;
    ud.loop_status = LOOP_NONE;
    ud.changed_properties = g_hash_table_new(g_str_hash, g_str_equal);
    ud.seek_expected = FALSE;

    ud.bus_id = g_bus_own_name(G_BUS_TYPE_SESSION,
                               "org.mpris.MediaPlayer2.mpv",
                               G_BUS_NAME_OWNER_FLAGS_NONE,
                               on_bus_acquired,
                               NULL,
                               on_name_lost,
                               &ud, NULL);

    // Receive event for property changes
    mpv_observe_property(mpv, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv, 0, "media-title", MPV_FORMAT_STRING);
    mpv_observe_property(mpv, 0, "speed", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "volume", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "loop-file", MPV_FORMAT_STRING);
    mpv_observe_property(mpv, 0, "loop-playlist", MPV_FORMAT_STRING);
    mpv_observe_property(mpv, 0, "duration", MPV_FORMAT_INT64);

    // Run callback whenever there are events
    g_unix_open_pipe(pipe, 0, &error);
    if (error != NULL) {
        g_printerr("%s", error->message);
    }
    fcntl(pipe[0], F_SETFL, O_NONBLOCK);
    mpv_set_wakeup_callback(mpv, wakeup_handler, &pipe[1]);
    mpv_pipe_source = g_unix_fd_add(pipe[0], G_IO_IN, event_handler, &ud);

    // Emit any new property changes every 100ms
    timeout_source = g_timeout_add(100, emit_property_changes, &ud);

    g_main_loop_run(loop);

    g_source_remove(mpv_pipe_source);
    g_source_remove(timeout_source);

    g_dbus_connection_unregister_object(ud.connection, ud.root_interface_id);
    g_dbus_connection_unregister_object(ud.connection, ud.player_interface_id);

    g_bus_unown_name(ud.bus_id);
    g_main_loop_unref(loop);
    g_dbus_node_info_unref(introspection_data);

    return 0;
}
