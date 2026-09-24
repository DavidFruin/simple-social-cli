# Simple Social CLI — Architecture Map

> **Alternate IO for Simple Social (`https://app.davidfruin.com/api.php`)**  
> **This repo:** `simple-social-cli` — C, libcurl, hand-rolled JSON. Builds to `lib/libss.so` + `simple-social-cli`.

---

## 1. Why a CLI

The web app (`simple-social` repo) is the canonical IO; this repo is a *second* IO over the same `api.php`/`media.php`. It shares `users.jwt` so a login in either surface validates in both, and it aims for scriptability (`--json` bare) without re-implementing business logic.

---

## 2. File Tree (annotated)

```
simple-social-cli/
├── Makefile                   # all: lib → bin; libcurl via pkg-config or its SONAME
├── vendor/
│   └── include/curl/         # vendored 8.14.1 headers; only the runtime libcurl.so.4 is needed
├── lib/ → lib/libss.so
│   ├── ss_api.c/.h           # curl client, 40 endpoints, parse_* (see §4)
│   ├── ss_json.c/.h          # parser only: find_key, skip_string, json_get_*, array len/get
│   ├── ss_config.c/.h        # base_url, data_dir/Downloads, ~/.config/simple-social-cli/config.ini
│   ├── ss_state.c/.h         # JWT/user store in ~/.simple-social-cli/
│   └── ss_utils.c/.h         # str_trim/dup, url_encode
└── cli/
    ├── main.c                # auto_login, 28 commands, --json/--color globals, bounded joins
    └── output.c/.h           # human tables vs bare JSON, color, null-media guard
```

`*.o`, `*.so`, `*.a` ignored.

---

## 3. Build

```bash
make -j4          # lib/libss.a, then bin (libss.a + `pkg-config --libs libcurl`, else -l:libcurl.so.4)
make clean
./simple-social-cli --help
```

`--json` and `--color` are parsed **before** the command name (user req §2): `cli --json posts` works, `cli posts --json` does not — flags are global, not per-command.

---

## 4. Library (`lib/libss.so`)

### 4.1 Transport

`api_call(action, params, resp, size)`:
* `curl_easy_init`, `write_callback` (realloc), `post_fields = "action=foo&"+params`,
* `headers = Content-Type + Authorization: Bearer <g_jwt>` if set,
* `CURLOPT_URL = config_get_base_url()` (`https://app.davidfruin.com/api.php` default), `CURLOPT_POSTFIELDS`, `CURLOPT_WRITEFUNCTION`, `TIMEOUT 30`.
* `api_get_last_error()` mirrors server `"message"`/`"error"`; `json_get_int` now tolerates quoted numbers (`"48"`).

Media is special: `api_upload_media[_with_id]` builds a `curl_mime` (`file` + `action=uploadMedia`) to `…/media.php`; `api_delete_media` POSTs `action=deleteMedia&mediaId=…` to the same endpoint.

### 4.2 Endpoints (40)

Auth: login/logout, getMyInfo/getUserInfo/getUsers/getUserEmails, register*, reset*, deleteAccount  
Posts: createPost, deletePost, getMyPosts/getUserPosts/fetchFollowedPosts, getPostById, getPostPreviews, getPostLikes, getPostCommentCounts, like/unlike  
Users: follow/unfollow/isFollowing/getMyFollows/getMyFollowers  
Comments: create/get/delete + counts  
Notifs: getNotifications/getUnseenCount/markSeen  
Media: upload (+with_id), delete

Five were orphaned until Phase C (`get_my_posts, get_user_posts, get_post_likes, get_post_comment_counts, get_user_emails`); `posts`/`likes` now wire the first three.

### 4.3 Parsers

`parse_posts()` etc. do `json_get_array(resp,"posts",&s,&e)` → `json_array_len` → loop `json_array_get_item` → copy slice → `json_get_string/bool/int` per field, `memset`+`strncpy`. Likes: `json_get_array("likes")` counts → `is_liked` if `g_user_id` in there.

`mediaUrl` `"null"` string is normalized to `""` after `json_get_string` (both in `parse_posts` and `getPostById`) so `output.c:is_null_media` treats it as absent.

`follows/followers` parse `timestamp` then fallback `created_at` into `created_at` field (fixes phantom “Joined” blank).

---

## 5. CLI Binary

### 5.1 Startup

```
main(argc,argv)
  next_arg scans --help/--color/--json before command
  g_json_enabled ? color=0 : color = isatty(STDERR)
  output_init()
  if cmd not in {login,register,send-otp,reset-password}: auto_login()
    config_load() → api_init() → ss_state_load_jwt → api_set_jwt → api_get_my_info → ss_state_set_user
```

Every auth-gated `cmd_*` starts with `require_auth()` → `print_error` + `{"ok":false,…}` in json.

### 5.2 Commands (28)

Same table as root README; the diff vs backend §6:
* `posts [user_id] [--limit N --offset N]` → `posts` gap closed (was only `feed` = followed); bare `posts` = `getMyPosts`, with id = `getUserPosts`.
* `likes <postId>` → `getPostLikes` (bare JSON passthrough).
* `create <text> [--media <file>]` → `upload_with_id` then `createPost(mediaUrl)` with rollback `deleteMedia` on post failure (Phase M). No standalone `upload`.

### 5.3 Text handling

`cmd_create`/`cmd_comment` no longer `strcat` unbounded — bounded `memcpy` caps at `5000` (server limit). `url_encode` percent-encodes before `build_params`.

### 5.4 Output

`output.c`:
* `json_escape` escapes `"` `\` `\n\r\t` + `\u00xx` for `<0x20`.
* human: `print_posts` truncates text 40/`...`, dims separator, green for `is_liked`; `print_post` shows `Media:` only if not null; `print_users` table with Joined; etc. — all on **stdout** except errors.
* json: `g_json_enabled` branches: `print_posts → {"posts":[{id,text,timestamp,userID,userEmail,likeCount,isLiked,mediaUrl,likes}],hasMore,totalCount}`, `print_post → {"post":{…}}`, `print_users → {"users":[…]}`, `print_comments → {"comments":[…],hasMore,totalCount}`, `print_notifications → {"notifications":[…]}`, `print_profile → {"id",…}`, `print_count → {"count":N}`. Errors also to stdout `{"ok":false…}`.

---

## 6. State & config

* `ss_state.c`: `get_data_path() = $HOME/.simple-social-cli`, `ensure_data_dir()`, `jwt.txt` + `user.json` (`{"id", "email", "created_at"}`). `logout` in `main.c` unlinks both files.
* `ss_config.c`: `ensure_dirs` makes `~/.simple-social-cli` + `~/.config/simple-social-cli`; `set_defaults` sets `data_dir = ~/.simple-social-cli`, `download_dir = ~/Downloads`, `base_url = https://app.davidfruin.com/api.php`; `config_load` reads `~/.config/simple-social-cli/config.ini`.
* Server `private/.env` is unrelated — CLI holds the client JWT, server holds the HMAC secret. `JWT_SECRET` never enters this repo.

---

## 7. Data flows (CLI perspective)

**Create with media:**
```
argv: create "hi" --media ./a.jpg
  parse --media, join text (≤5000) → api_upload_media_with_id(a.jpg) → curl_mime POST /media.php → {mediaUrl, mediaId}
  → api_create_post(text, mediaUrl) → POST /api.php action=post
     server validates SELECT media WHERE path=? AND user_id=? → 400 if forged, else UPDATE media SET post_id
  → on post error api_delete_media(mediaId) rollback
  → {"postId":…, "mediaUrl":…}
```

**Read post (Phase A):**
```
post 1.123 → api_get_post_by_id → POST action=getPostById → server SELECT posts WHERE id=owner, injects userID/userEmail from users.email → {"post":{id,text,timestamp,userID,userEmail,likeCount,isLiked,mediaUrl}}
```

**Feed vs posts:**
```
feed → fetchFollowedPosts (merged)
posts → getMyPosts (self) / getUserPosts (other) — the missing primitive before Phase C
```

---

## 8. Design deltas (this repo)

* B: `vendor/libcurl.so` restored, `Makefile` rpath + parallel fix, bounded joins, `--json` bare, color fix, media null + timestamp fixes, `commentId` string-int.
* M: `upload` removed, `create --media` + rollback, `delete_media`/`upload_with_id` added.
* C: `posts` + `likes` wired.
* `tui → cli` storage rename. The migration fallbacks (`~/.simple-social-tui`, `~/.config/simple-social-tui`) were removed once migration was done, freeing the `simple-social-tui` name for the separate TUI front end.

---

## 9. Gotchas

* Post ID = second-granularity → same-second collision (two `create` in same second get same id, last write wins). CLI bounded join will truncate rather than overflow.
* `--media` file must exist locally; server still enforces type (jpg/png/gif/webp/mov/mp4/m4v/wav/mp3/webm) and size (10/100/50 MB).
* Old test orphans at `/media/1/image/1_image_20260915013530_*` pre-date `post_id` linkage — intentionally not backfilled.

---

*See `simple-social` repo `ARCHITECTURE.md` for backend + frontend deep dive and private/ deployment map.*
