#include "internal.h"
#include <math.h>

#define VEC_DIM 16

typedef struct {
    const char *word;
    float vec[VEC_DIM];
} WordVector;

typedef struct {
    const char *from;
    const char *to;
    float weight;
} TransitionPair;

/* Bảng vector ngữ nghĩa 16 chiều cho các từ khóa tiếng Việt cốt lõi */
static const WordVector g_word_vectors[] = {
    /* Xã giao / Chào hỏi / Lịch sự */
    {"xin",         {0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"chào",        {0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"cảm",         {0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.4f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"ơn",          {0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.4f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"chúc",        {0.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.2f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.6f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"mừng",        {0.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.2f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.6f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"lỗi",         {0.7f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, -0.4f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"phép",        {0.7f, 0.0f, 0.0f, 0.3f, 0.0f, 0.0f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},

    /* Công nghệ / Phần mềm / Hệ thống */
    {"công",        {0.0f, 0.8f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"nghệ",        {0.0f, 0.9f, 0.4f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"thông",       {0.0f, 0.8f, 0.3f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"tin",         {0.0f, 0.8f, 0.3f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"phần",        {0.0f, 0.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f}},
    {"mềm",         {0.0f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f}},
    {"hệ",          {0.0f, 0.7f, 0.3f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.4f, 0.0f, 0.0f, 0.0f}},
    {"thống",       {0.0f, 0.7f, 0.3f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.4f, 0.0f, 0.0f, 0.0f}},
    {"máy",         {0.0f, 0.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.6f, 0.0f, 0.0f, 0.0f}},
    {"tính",        {0.0f, 0.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.6f, 0.0f, 0.0f, 0.0f}},
    {"dữ",          {0.0f, 0.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.3f, 0.0f, 0.0f, 0.0f}},
    {"liệu",        {0.0f, 0.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.3f, 0.0f, 0.0f, 0.0f}},
    {"mạng",        {0.0f, 0.8f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.3f, 0.0f, 0.0f, 0.0f}},

    /* Kinh tế / Phát triển / Xã hội */
    {"kinh",        {0.0f, 0.2f, 0.9f, 0.4f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"tế",          {0.0f, 0.2f, 0.9f, 0.4f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"phát",        {0.0f, 0.4f, 0.8f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"triển",       {0.0f, 0.4f, 0.8f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"thị",         {0.0f, 0.0f, 0.8f, 0.2f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"trường",      {0.0f, 0.0f, 0.8f, 0.2f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"quản",        {0.0f, 0.3f, 0.7f, 0.4f, 0.0f, 0.0f, 0.0f, 0.0f, 0.4f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"lý",          {0.0f, 0.3f, 0.7f, 0.4f, 0.0f, 0.0f, 0.0f, 0.0f, 0.4f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"xã",          {0.0f, 0.0f, 0.4f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"hội",         {0.0f, 0.0f, 0.4f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"cộng",        {0.0f, 0.0f, 0.3f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"hòa",         {0.0f, 0.0f, 0.3f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"chủ",         {0.0f, 0.0f, 0.3f, 0.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"nghĩa",       {0.0f, 0.0f, 0.3f, 0.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"quốc",        {0.0f, 0.0f, 0.3f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"gia",         {0.0f, 0.0f, 0.3f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"việt",        {0.0f, 0.0f, 0.3f, 0.9f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"nam",         {0.0f, 0.0f, 0.3f, 0.9f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},

    /* Địa lý / Thành phố */
    {"thành",       {0.0f, 0.0f, 0.2f, 0.3f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"phố",         {0.0f, 0.0f, 0.2f, 0.3f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"hà",          {0.0f, 0.0f, 0.0f, 0.3f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"nội",         {0.0f, 0.0f, 0.0f, 0.3f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"hồ",          {0.0f, 0.0f, 0.0f, 0.3f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"chí",         {0.0f, 0.0f, 0.0f, 0.3f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"minh",        {0.0f, 0.0f, 0.0f, 0.3f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"đà",          {0.0f, 0.0f, 0.0f, 0.2f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"nẵng",        {0.0f, 0.0f, 0.0f, 0.2f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},

    /* Thời gian */
    {"hôm",         {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"nay",         {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"qua",         {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.8f, 0.0f, 0.0f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"mai",         {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"ngày",        {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"tháng",       {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"năm",         {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"thời",        {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"gian",        {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},

    /* Ngôn ngữ / Bộ gõ */
    {"tiếng",       {0.3f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.7f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"anh",         {0.0f, 0.0f, 0.0f, 0.3f, 0.0f, 0.0f, 0.6f, 0.0f, 0.0f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"bộ",          {0.0f, 0.6f, 0.2f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f}},
    {"gõ",          {0.0f, 0.7f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.4f, 0.0f, 0.0f, 0.0f}},
    {"bàn",         {0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.8f, 0.0f, 0.0f, 0.0f}},
    {"phím",        {0.0f, 0.6f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.8f, 0.0f, 0.0f, 0.0f}},

    /* Động từ và cấu trúc */
    {"học",         {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.2f, 0.0f, 0.6f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"tập",         {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.2f, 0.0f, 0.6f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"sinh",        {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.6f, 0.0f, 0.0f, 0.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"làm",         {0.0f, 0.0f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"việc",        {0.0f, 0.0f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"nghiên",      {0.0f, 0.6f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.6f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"cứu",         {0.0f, 0.6f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.6f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},

    /* Đại từ */
    {"chúng",       {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"tôi",         {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"ta",          {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.9f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
    {"bạn",         {0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f}},

    {NULL, {0}}
};

/* Bảng liên kết chuyển tiếp từ (Bigram / Trigram Collocation Affinity) */
static const TransitionPair g_transitions[] = {
    {"xin", "chào", 0.98f},
    {"xin", "cảm ơn", 0.95f},
    {"xin", "lỗi", 0.92f},
    {"xin", "phép", 0.88f},
    {"cảm", "ơn", 0.99f},
    {"cảm", "thấy", 0.85f},
    {"cảm", "nhận", 0.82f},
    {"cảm", "xúc", 0.80f},
    {"chúc", "mừng", 0.99f},
    {"chúc", "bạn", 0.88f},
    {"chúc", "ngủ ngon", 0.85f},
    {"mừng", "năm mới", 0.98f},
    {"mừng", "sinh nhật", 0.95f},
    {"mừng", "thành công", 0.90f},
    {"công nghệ", "thông tin", 0.99f},
    {"công nghệ", "mới", 0.88f},
    {"công nghệ", "cao", 0.85f},
    {"thông tin", "chi tiết", 0.90f},
    {"thông tin", "mới nhất", 0.88f},
    {"thông tin", "liên hệ", 0.85f},
    {"phần mềm", "máy tính", 0.92f},
    {"phần mềm", "nguồn mở", 0.90f},
    {"phần mềm", "hệ thống", 0.88f},
    {"hệ thống", "thông tin", 0.95f},
    {"hệ thống", "quản lý", 0.92f},
    {"hệ thống", "máy tính", 0.88f},
    {"phát triển", "kinh tế", 0.96f},
    {"phát triển", "bền vững", 0.93f},
    {"phát triển", "phần mềm", 0.90f},
    {"phát triển", "mạnh mẽ", 0.88f},
    {"kinh tế", "xã hội", 0.97f},
    {"kinh tế", "thị trường", 0.94f},
    {"kinh tế", "phát triển", 0.90f},
    {"cộng hòa", "xã hội", 0.99f},
    {"xã hội", "chủ nghĩa", 0.99f},
    {"chủ nghĩa", "việt nam", 0.95f},
    {"quốc gia", "việt nam", 0.92f},
    {"thành phố", "hồ chí minh", 0.99f},
    {"thành phố", "hà nội", 0.96f},
    {"thành phố", "đà nẵng", 0.93f},
    {"hồ chí", "minh", 0.99f},
    {"hà", "nội", 0.99f},
    {"đà", "nẵng", 0.99f},
    {"tiếng", "việt", 0.99f},
    {"tiếng", "anh", 0.95f},
    {"tiếng", "nói", 0.82f},
    {"bộ", "gõ", 0.98f},
    {"gõ", "tiếng việt", 0.98f},
    {"bàn", "phím", 0.98f},
    {"hôm", "nay", 0.99f},
    {"hôm", "qua", 0.96f},
    {"hôm", "kia", 0.90f},
    {"ngày", "mai", 0.98f},
    {"ngày", "nay", 0.95f},
    {"ngày", "hôm nay", 0.92f},
    {"thời", "gian", 0.99f},
    {"học", "tập", 0.97f},
    {"học", "sinh", 0.95f},
    {"học", "hành", 0.90f},
    {"làm", "việc", 0.98f},
    {"làm", "sao", 0.88f},
    {"làm", "quen", 0.85f},
    {"nghiên", "cứu", 0.99f},
    {"chúng", "tôi", 0.98f},
    {"chúng", "ta", 0.95f},
    {"chúng", "mình", 0.90f},
    {"có", "thể", 0.96f},
    {"có", "phải", 0.90f},
    {"không", "thể", 0.96f},
    {"không", "có", 0.92f},
    {"không", "phải", 0.90f},
    {NULL, NULL, 0.0f}
};

static float cosine_similarity(const float *a, const float *b, int dim){
    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for(int i=0; i<dim; i++){
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    if(norm_a <= 0.00001f || norm_b <= 0.00001f) return 0.0f;
    return dot / (sqrtf(norm_a) * sqrtf(norm_b));
}

static const float *find_word_vector(const char *word){
    for(int i=0; g_word_vectors[i].word; i++){
        if(strcmp(word, g_word_vectors[i].word) == 0) return g_word_vectors[i].vec;
    }
    return NULL;
}

typedef struct {
    char *word;
    float score;
} VectorCand;

static void add_vector_cand(GArray *cands, const char *word, float score){
    if(!word || !*word) return;
    for(guint i=0; i<cands->len; i++){
        VectorCand *vc = &g_array_index(cands, VectorCand, i);
        if(strcmp(vc->word, word) == 0){
            if(score > vc->score) vc->score = score;
            return;
        }
    }
    VectorCand vc = {.word = g_strdup(word), .score = score};
    g_array_append_val(cands, vc);
}

static gint compare_vector_cand(gconstpointer a, gconstpointer b){
    const VectorCand *va = a;
    const VectorCand *vb = b;
    if(vb->score > va->score) return 1;
    if(vb->score < va->score) return -1;
    return strcmp(va->word, vb->word);
}

/* Áp dụng TitleCase nếu ngữ cảnh viết hoa */
static gchar *format_casing(const char *template_ctx, const char *word){
    if(!template_ctx || !word) return g_strdup(word ? word : "");
    gunichar first = g_utf8_get_char(template_ctx);
    if(g_unichar_isupper(first)){
        glong wlen = 0;
        gunichar *wu = g_utf8_to_ucs4(word, -1, NULL, &wlen, NULL);
        if(wu && wlen > 0){
            wu[0] = g_unichar_toupper(wu[0]);
            gchar *res = g_ucs4_to_utf8(wu, wlen, NULL, NULL, NULL);
            g_free(wu);
            if(res) return res;
        }
    }
    return g_strdup(word);
}

GPtrArray *gtv_vector_predict_next(const gchar *context, const gchar *prefix, guint max_results){
    GPtrArray *results = g_ptr_array_new_with_free_func(g_free);
    if(!context || !*context) return results;

    gchar *ctx_lower = g_utf8_strdown(context, -1);
    g_strstrip(ctx_lower);
    gchar *pfx_lower = prefix ? g_utf8_strdown(prefix, -1) : NULL;
    if(pfx_lower) g_strstrip(pfx_lower);

    /* Tách các từ trong ngữ cảnh */
    gchar **tokens = g_strsplit_set(ctx_lower, " ,.?!;:\t\n", -1);
    guint n_tokens = g_strv_length(tokens);
    if(n_tokens == 0){
        g_strfreev(tokens); g_free(ctx_lower); g_free(pfx_lower);
        return results;
    }

    const gchar *last1 = tokens[n_tokens - 1];
    gchar *last2 = NULL;
    if(n_tokens >= 2){
        last2 = g_strdup_printf("%s %s", tokens[n_tokens - 2], tokens[n_tokens - 1]);
    }

    GArray *cands = g_array_new(FALSE, FALSE, sizeof(VectorCand));

    /* 1. Ưu tiên liên kết ngữ nghĩa chuyển tiếp trực tiếp (Transition Collocation) */
    for(int i=0; g_transitions[i].from; i++){
        gboolean match = FALSE;
        if(last2 && strcmp(last2, g_transitions[i].from) == 0){
            match = TRUE;
        } else if(strcmp(last1, g_transitions[i].from) == 0){
            match = TRUE;
        }

        if(match){
            const char *target = g_transitions[i].to;
            if(!pfx_lower || !*pfx_lower || g_str_has_prefix(target, pfx_lower)){
                /* Điểm cao hơn cho bigram 2 từ */
                float bonus = (last2 && strcmp(last2, g_transitions[i].from) == 0) ? 0.2f : 0.0f;
                add_vector_cand(cands, target, g_transitions[i].weight + bonus);
            }
        }
    }

    /* 2. Tính tương đồng Vector trong không gian ngữ nghĩa 16 chiều */
    const float *v_last = find_word_vector(last1);
    if(v_last){
        for(int i=0; g_word_vectors[i].word; i++){
            const char *target = g_word_vectors[i].word;
            if(strcmp(target, last1) == 0) continue;
            if(pfx_lower && *pfx_lower && !g_str_has_prefix(target, pfx_lower)) continue;

            float sim = cosine_similarity(v_last, g_word_vectors[i].vec, VEC_DIM);
            if(sim > 0.6f){
                add_vector_cand(cands, target, sim * 0.8f);
            }
        }
    }

    /* 3. Sắp xếp các ứng viên theo độ tương đồng giảm dần */
    g_array_sort(cands, compare_vector_cand);

    /* 4. Trích xuất top kết quả và áp dụng định dạng hoa/thường */
    for(guint i=0; i<cands->len && results->len < max_results; i++){
        VectorCand *vc = &g_array_index(cands, VectorCand, i);
        gchar *formatted = format_casing(context, vc->word);
        g_ptr_array_add(results, formatted);
    }

    /* Dọn dẹp */
    for(guint i=0; i<cands->len; i++){
        VectorCand *vc = &g_array_index(cands, VectorCand, i);
        g_free(vc->word);
    }
    g_array_unref(cands);
    g_free(last2);
    g_strfreev(tokens);
    g_free(ctx_lower);
    g_free(pfx_lower);

    return results;
}
