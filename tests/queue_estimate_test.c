/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/auth/queue_estimate.h"
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "queue_estimate_test:%d: %s\n", __LINE__, #expr); return 1; \
} } while (0)

static void parse(const char *body, int status, X4QueueEstimate *estimate)
{
    x4_queue_estimate_parse(body, body ? strlen(body) : 0, status, estimate);
}

static int test_url(void)
{
    char url[512];
    CHECK(x4_queue_waittime_url("https://westus2.gssv-play-prod.xboxlive.com",
        "9NBLGGH4R315", url, sizeof(url)));
    CHECK(!strcmp(url, "https://westus2.gssv-play-prod.xboxlive.com/v1/waittime/9NBLGGH4R315"));
    CHECK(x4_queue_waittime_url_valid(url));
    CHECK(!x4_queue_waittime_url_valid("http://westus2.gssv-play-prod.xboxlive.com/v1/waittime/X"));
    CHECK(!x4_queue_waittime_url_valid("https://westus2.gssv-play-prod.xboxlive.com.evil/v1/waittime/X"));
    CHECK(!x4_queue_waittime_url_valid("https://user@westus2.gssv-play-prod.xboxlive.com/v1/waittime/X"));
    CHECK(!x4_queue_waittime_url_valid("https://westus2.gssv-play-prod.xboxlive.com:443/v1/waittime/X"));
    CHECK(!x4_queue_waittime_url_valid("https://westus2.gssv-play-prod.xboxlive.com/v1/waittime/X?x=1"));
    CHECK(!x4_queue_waittime_url_valid("https://westus2.gssv-play-prod.xboxlive.com/v1/waittime/X/Y"));
    CHECK(!x4_queue_waittime_url_valid("https://westus2.gssv-play-prod.xboxlive.com/v1/waittime/.."));
    CHECK(!x4_queue_waittime_url("https://westus2.gssv-play-prod.xboxlive.com", "../X", url, sizeof(url)));
    CHECK(!url[0]);
    CHECK(!x4_queue_waittime_url("https://westus2.gssv-play-prod.xboxlive.com", "X", url, 8));
    CHECK(!url[0]);
    return 0;
}

static int test_optional_and_stale(void)
{
    X4QueueEstimate e = {0};
    CHECK(e.result == X4_QUEUE_NONE && e.present_mask == 0);
    parse("{}", 200, &e);
    CHECK(e.result == X4_QUEUE_UNAVAILABLE && e.present_mask == 0);
    parse("{\"estimatedTotalWaitTimeInSeconds\":0}", 200, &e);
    CHECK(e.result == X4_QUEUE_AVAILABLE && e.present_mask == X4_QUEUE_TOTAL);
    CHECK(e.total_seconds == 0 && e.allocation_seconds == 0);
    parse("{\"estimatedAllocationTimeInSeconds\":120,\"estimatedProvisioningTimeInSeconds\":35,\"estimatedTotalWaitTimeInSeconds\":155}", 200, &e);
    CHECK(e.present_mask == (X4_QUEUE_ALLOCATION | X4_QUEUE_PROVISIONING | X4_QUEUE_TOTAL));
    CHECK(e.allocation_seconds == 120 && e.provisioning_seconds == 35 && e.total_seconds == 155);
    e.updated_elapsed_seconds = 40;
    e.query_count = 2;
    parse(NULL, 0, &e);
    CHECK(e.result == X4_QUEUE_ERROR && e.http_status == 0);
    CHECK(e.present_mask == 7 && e.total_seconds == 155);
    CHECK(e.updated_elapsed_seconds == 40 && e.query_count == 2);
    parse("", 429, &e);
    CHECK(e.result == X4_QUEUE_UNAVAILABLE && e.http_status == 429);
    CHECK(e.present_mask == 7 && e.total_seconds == 155 && e.updated_elapsed_seconds == 40);
    parse("{}", 503, &e);
    CHECK(e.result == X4_QUEUE_ERROR && e.http_status == 503 && e.present_mask == 7);
    parse("{}", 200, &e);
    CHECK(e.result == X4_QUEUE_UNAVAILABLE && e.present_mask == 0);
    return 0;
}

static int test_invalid(void)
{
    const char *invalid[] = {
        "{\"estimatedTotalWaitTimeInSeconds\":-1}",
        "{\"estimatedTotalWaitTimeInSeconds\":1.5}",
        "{\"estimatedTotalWaitTimeInSeconds\":\"20\"}",
        "{\"estimatedTotalWaitTimeInSeconds\":86401}",
        "{\"estimatedTotalWaitTimeInSeconds\":2,\"estimatedTotalWaitTimeInSeconds\":3}",
        "[]", "{not-json}"
    };
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        X4QueueEstimate e = {0};
        parse(invalid[i], 200, &e);
        CHECK(e.result == X4_QUEUE_ERROR && e.present_mask == 0);
    }
    X4QueueEstimate e = {0};
    parse("{\"estimatedAllocationTimeInSeconds\":86400}", 200, &e);
    CHECK(e.result == X4_QUEUE_AVAILABLE && e.present_mask == X4_QUEUE_ALLOCATION);
    CHECK(e.allocation_seconds == 86400);
    return 0;
}

static int test_refresh_policy(void)
{
    CHECK(!x4_queue_refresh_due(false, 100000000, 0));
    CHECK(x4_queue_refresh_due(true, 0, 0));
    CHECK(!x4_queue_refresh_due(true, 29999999, 30000000));
    CHECK(x4_queue_refresh_due(true, 30000000, 30000000));
    return 0;
}

int main(void)
{
    CHECK(test_url() == 0);
    CHECK(test_optional_and_stale() == 0);
    CHECK(test_invalid() == 0);
    CHECK(test_refresh_policy() == 0);
    puts("queue_estimate_test: all cases passed");
    return 0;
}
