/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/auth/region_selection.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int select_json(const char *json, const char *id, X4RegionSnapshot *view, char *origin)
{
    X4JsonSpan root;
    assert(x4_json_parse(json, strlen(json), &root) == 0);
    return x4_region_select(root, id, view, origin, X4_REGION_ORIGIN_SIZE);
}

static void origins(void)
{
    char out[X4_REGION_ORIGIN_SIZE];
    assert(!x4_region_origin("https://WestUs.GSSV-PLAY-PROD.XBOXLIVE.COM/", out, sizeof(out)));
    assert(!strcmp(out, "https://westus.gssv-play-prod.xboxlive.com"));
    const char *bad[] = {
        "http://westus.gssv-play-prod.xboxlive.com", "https://evil.example",
        "https://westus.gssv-play-prod.xboxlive.com.evil.example",
        "https://user@westus.gssv-play-prod.xboxlive.com",
        "https://westus.gssv-play-prod.xboxlive.com:443",
        "https://westus.gssv-play-prod.xboxlive.com/path",
        "https://westus.gssv-play-prod.xboxlive.com//",
        "https://westus.gssv-play-prod.xboxlive.com?query",
        "https://westus.gssv-play-prod.xboxlive.com/#fragment",
        "https://.westus.gssv-play-prod.xboxlive.com",
        "https://west..gssv-play-prod.xboxlive.com",
        "https://-west.gssv-play-prod.xboxlive.com",
        "https://west-.gssv-play-prod.xboxlive.com",
        "https://west_us.gssv-play-prod.xboxlive.com",
        "https://gssv-play-prod.xboxlive.com",
        "https://west.gssv-play-prod.xboxlive.com.",
        "https://aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.gssv-play-prod.xboxlive.com"
    };
    for (size_t i = 0; i < sizeof(bad)/sizeof(bad[0]); ++i) {
        strcpy(out, "stale");
        assert(x4_region_origin(bad[i], out, sizeof(out)) == -1);
        assert(!out[0]);
    }
    assert(x4_region_origin("https://west.gssv-play-prod.xboxlive.com", out, 8) == -1);
    assert(!out[0]);
}

static void selection(void)
{
    const char json[] = "{\"offeringSettings\":{\"regions\":["
        "{\"name\":\"Bad\",\"baseUri\":\"https://evil.example\",\"isDefault\":true},"
        "{\"name\":\"WestUs\",\"baseUri\":\"https://west.gssv-play-prod.xboxlive.com/\",\"isDefault\":true},"
        "{\"name\":\"Brazil South\",\"baseUri\":\"https://brazil.gssv-play-prod.xboxlive.com\",\"isDefault\":false},"
        "{\"baseUri\":\"https://unnamed.gssv-play-prod.xboxlive.com\"}"
        "]}}";
    X4RegionSnapshot view;
    char origin[X4_REGION_ORIGIN_SIZE];
    assert(!select_json(json, NULL, &view, origin));
    assert(view.count == 3 && !view.truncated && !view.fallback && !view.preferred_id[0]);
    assert(!strcmp(view.active_id, "WestUs") && view.items[0].is_default);
    assert(!strcmp(origin, "https://west.gssv-play-prod.xboxlive.com"));
    assert(!strcmp(view.items[2].id, "unnamed"));
    assert(!select_json(json, "Brazil South", &view, origin));
    assert(!strcmp(view.active_id, "Brazil South") && !view.fallback);
    assert(!strcmp(origin, "https://brazil.gssv-play-prod.xboxlive.com"));
    assert(!select_json(json, "Absent", &view, origin));
    assert(view.fallback && !strcmp(view.preferred_id, "Absent") && !strcmp(view.active_id, "WestUs"));
    assert(!select_json(json, "", &view, origin) && !view.fallback);
    assert(select_json(json, "https://evil.example", &view, origin) == -1);
    assert(!view.count && !origin[0]);
    const char *missing[] = {"{}", "{\"offeringSettings\":{}}", "{\"offeringSettings\":{\"regions\":{}}}",
        "{\"offeringSettings\":{\"regions\":[]}}",
        "{\"offeringSettings\":{\"regions\":[{\"name\":\"Only\",\"baseUri\":\"https://only.gssv-play-prod.xboxlive.com\"}]}}"};
    for (size_t i = 0; i < sizeof(missing)/sizeof(missing[0]); ++i) {
        memset(&view, 0x7f, sizeof(view)); strcpy(origin, "stale");
        assert(select_json(missing[i], "", &view, origin) == -1);
        assert(!view.count && !view.active_id[0] && !origin[0]);
    }
    assert(!select_json(missing[4], "Only", &view, origin));
    assert(!view.fallback && !strcmp(view.active_id, "Only"));
    const char *duplicate = "{\"offeringSettings\":{\"regions\":["
        "{\"name\":\"Same\",\"baseUri\":\"https://first.gssv-play-prod.xboxlive.com\",\"isDefault\":true},"
        "{\"name\":\"Same\",\"baseUri\":\"https://second.gssv-play-prod.xboxlive.com\"}]}}";
    assert(!select_json(duplicate, "Same", &view, origin));
    assert(view.count == 1 && !strcmp(origin, "https://first.gssv-play-prod.xboxlive.com"));
    char long_id[X4_REGION_ID_SIZE + 1]; memset(long_id, 'a', sizeof(long_id)); long_id[sizeof(long_id)-1] = 0;
    assert(!x4_region_id_valid(long_id));
    assert(!x4_region_id_valid(" bad") && !x4_region_id_valid("bad ") && !x4_region_id_valid("bad\n"));
}

/* Catalog and session must resolve the identical saved ID against their own
 * current offering login, including changed order, endpoint and availability. */
static void fresh_session_selection(void)
{
    const char *catalog = "{\"offeringSettings\":{\"regions\":["
        "{\"name\":\"Default\",\"baseUri\":\"https://default.gssv-play-prod.xboxlive.com\",\"isDefault\":true},"
        "{\"name\":\"Chosen\",\"baseUri\":\"https://old.gssv-play-prod.xboxlive.com\"}]}}";
    const char *session = "{\"offeringSettings\":{\"regions\":["
        "{\"name\":\"Chosen\",\"baseUri\":\"https://fresh.gssv-play-prod.xboxlive.com\"},"
        "{\"name\":\"Default\",\"baseUri\":\"https://default.gssv-play-prod.xboxlive.com\",\"isDefault\":true}]}}";
    const char *other_offering = "{\"offeringSettings\":{\"regions\":["
        "{\"name\":\"Other\",\"baseUri\":\"https://other.gssv-play-prod.xboxlive.com\",\"isDefault\":true}]}}";
    X4RegionSnapshot view; char origin[X4_REGION_ORIGIN_SIZE];
    assert(!select_json(catalog, "Chosen", &view, origin));
    assert(!strcmp(view.active_id, "Chosen") && !strcmp(origin, "https://old.gssv-play-prod.xboxlive.com"));
    assert(!select_json(session, "Chosen", &view, origin));
    assert(!strcmp(view.active_id, "Chosen") && !strcmp(origin, "https://fresh.gssv-play-prod.xboxlive.com"));
    assert(!select_json(other_offering, "Chosen", &view, origin));
    assert(view.count == 1 && view.fallback && !strcmp(view.active_id, "Other"));
    assert(!strcmp(origin, "https://other.gssv-play-prod.xboxlive.com"));
}

static void bounds(void)
{
    char json[65536];
    for (unsigned count = X4_REGION_MAX + 8; count <= X4_REGION_SCAN_MAX + 1; count += X4_REGION_SCAN_MAX + 1 - (X4_REGION_MAX + 8)) {
        size_t n = (size_t)snprintf(json, sizeof(json), "{\"offeringSettings\":{\"regions\":[");
        for (unsigned i = 0; i < count; ++i)
            n += (size_t)snprintf(json + n, sizeof(json) - n,
                "%s{\"name\":\"R%u\",\"baseUri\":\"https://r%u.gssv-play-prod.xboxlive.com\",\"isDefault\":%s}",
                i ? "," : "", i, i, i == count-1 ? "true" : "false");
        assert(n + 4 < sizeof(json)); strcpy(json + n, "]}}");
        X4RegionSnapshot view; char origin[X4_REGION_ORIGIN_SIZE], id[32];
        snprintf(id, sizeof(id), "R%u", count-1);
        if (count > X4_REGION_SCAN_MAX) {
            assert(select_json(json, id, &view, origin) == -1 && !view.count && !origin[0]);
        } else {
            assert(!select_json(json, "", &view, origin));
            assert(view.truncated && view.count == X4_REGION_MAX && !strcmp(view.active_id, id));
            assert(!strcmp(view.items[X4_REGION_MAX-1].id, id));
            assert(!select_json(json, "R35", &view, origin));
            assert(view.truncated && !view.fallback && !strcmp(view.active_id, "R35"));
            assert(!strcmp(view.items[X4_REGION_MAX-1].id, "R35"));
        }
    }
}

int main(void)
{
    origins(); selection(); fresh_session_selection(); bounds();
    puts("region_selection_test: ok");
    return 0;
}
