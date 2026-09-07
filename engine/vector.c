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

/* Normalize canonically before token and prefix comparisons. */
static gchar *normalize_text(const gchar *text){
    gchar *lower = g_utf8_strdown(text, -1);
    gchar *normalized = g_utf8_normalize(lower, -1, G_NORMALIZE_NFC);
    g_free(lower);
    return normalized;
}
static gchar *fold_accents(const gchar *text){
    gchar *decomposed = g_utf8_normalize(text, -1, G_NORMALIZE_NFD);
    GString *folded = g_string_new("");
    for(const gchar *p=decomposed; *p; p=g_utf8_next_char(p)){
        gunichar c=g_utf8_get_char(p);
        if(!g_unichar_ismark(c)) g_string_append_unichar(folded,c == 0x0111 ? 'd' : c);
    }
    g_free(decomposed);
    return g_string_free(folded,FALSE);
}
static gboolean prefix_matches(const gchar *target, const gchar *prefix, const gchar *folded_prefix){
    if(g_str_has_prefix(target,prefix)) return TRUE;
    /* Respect accents that the user has already explicitly entered. */
    if(strcmp(prefix,folded_prefix)) return FALSE;
    gchar *folded=fold_accents(target);
    gboolean match=g_str_has_prefix(folded,folded_prefix);
    g_free(folded);
    return match;
}
/* Keep at most eight words, and never carry semantic context across a sentence. */
static GPtrArray *context_tokens(const gchar *text){
    GPtrArray *tokens=g_ptr_array_new_with_free_func(g_free);
    GString *word=g_string_new("");
    for(const gchar *p=text;;p=g_utf8_next_char(p)){
        gunichar c=g_utf8_get_char(p);
        if(g_unichar_isalnum(c) || g_unichar_ismark(c)) g_string_append_unichar(word,c);
        else {
            if(word->len){
                if(tokens->len == 8) g_ptr_array_remove_index(tokens,0);
                g_ptr_array_add(tokens,g_strdup(word->str));
                g_string_truncate(word,0);
            }
            if(c && g_utf8_strchr(".!?;\n。！？",-1,c)) g_ptr_array_set_size(tokens,0);
        }
        if(!c) break;
    }
    g_string_free(word,TRUE);
    return tokens;
}
GPtrArray *gtv_vector_predict_next(const gchar *context, const gchar *prefix, guint max_results){
    GPtrArray *results=g_ptr_array_new_with_free_func(g_free);
    if(!max_results || !context || !g_utf8_validate(context,-1,NULL) ||
       (prefix && !g_utf8_validate(prefix,-1,NULL))) return results;
    gchar *ctx=normalize_text(context);
    gchar *typed=g_strdup(prefix ? prefix : "");g_strstrip(typed);
    gchar *pfx=normalize_text(typed), *folded=fold_accents(pfx);
    GPtrArray *tokens=context_tokens(ctx);
    GArray *cands=g_array_new(FALSE,FALSE,sizeof(VectorCand));
    if(!tokens->len) goto done;
    const gchar *last=g_ptr_array_index(tokens,tokens->len-1);
    gchar *pair=tokens->len>1 ? g_strjoin(" ",g_ptr_array_index(tokens,tokens->len-2),last,NULL) : NULL;

    /* Recent words contribute most; unknown tokens still consume a time step. */
    float context_vec[VEC_DIM]={0}, weight=1.0f;
    for(gint i=(gint)tokens->len-1;i>=0;i--,weight*=0.5f){
        const float *vec=find_word_vector(g_ptr_array_index(tokens,i));
        if(vec) for(guint j=0;j<VEC_DIM;j++) context_vec[j]+=weight*vec[j];
    }
    /* Direct phrase transitions always outrank semantic-only neighbors. */
    for(guint i=0;g_transitions[i].from;i++){
        const TransitionPair *t=&g_transitions[i];
        gboolean longer=pair && !strcmp(pair,t->from);
        if((longer || !strcmp(last,t->from)) && prefix_matches(t->to,pfx,folded)){
            float score=2.0f+t->weight+(longer ? 0.2f : 0);
            add_vector_cand(cands,t->to,score);
        }
    }
    g_free(pair);
    for(guint i=0;g_word_vectors[i].word;i++){
        const WordVector *target=&g_word_vectors[i];
        if(!strcmp(target->word,last) || !prefix_matches(target->word,pfx,folded)) continue;
        float similarity=cosine_similarity(context_vec,target->vec,VEC_DIM);
        if(similarity>0.6f) add_vector_cand(cands,target->word,similarity);
    }
    g_array_sort(cands,compare_vector_cand);
    for(guint i=0;i<cands->len && results->len<max_results;i++){
        const VectorCand *cand=&g_array_index(cands,VectorCand,i);
        /* A completed word is not a completion suggestion. */
        if(strcmp(cand->word,pfx)) g_ptr_array_add(results,g_strdup(cand->word));
    }
done:
    for(guint i=0;i<cands->len;i++) g_free(g_array_index(cands,VectorCand,i).word);
    g_array_unref(cands);g_ptr_array_unref(tokens);
    g_free(ctx);g_free(typed);g_free(pfx);g_free(folded);
    return results;
}
