#include <pebble.h>

static Window *s_main_window;
static TextLayer *s_time_layer;
static TextLayer *s_countdown_layer;
static TextLayer *s_name_layer;
static BitmapLayer *s_rocket_layer;
static GBitmap *s_rocket_bitmap;
static char s_countdown_buffer[64];
static char s_rocket_buffer[128];
static int32_t s_minutes_to_launch = 0;
static bool first_update;

static void inbox_received_callback(DictionaryIterator *iterator, void *context)
{
  Tuple *countdown_tuple = dict_find(iterator, MESSAGE_KEY_MINUTES_TO_LAUNCH);
  Tuple *rocket_tuple = dict_find(iterator, MESSAGE_KEY_ROCKET);

  if (countdown_tuple)
  {
    s_minutes_to_launch = countdown_tuple->value->int32;
    int hours = s_minutes_to_launch / 60;
    int minutes = s_minutes_to_launch % 60;
    snprintf(s_countdown_buffer, sizeof(s_countdown_buffer), "T-%02d:%02d", hours, minutes);
    if (s_minutes_to_launch > 0)
      text_layer_set_text(s_countdown_layer, s_countdown_buffer);
    else
      text_layer_set_text(s_countdown_layer, "Lift off!");
  }

  if (rocket_tuple)
  {
    snprintf(s_rocket_buffer, sizeof(s_rocket_buffer), "%s", rocket_tuple->value->cstring);
    text_layer_set_text(s_name_layer, s_rocket_buffer);
  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context)
{
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context)
{
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context)
{
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

static void update_time()
{
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  static char s_buffer[8];
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);

  text_layer_set_text(s_time_layer, s_buffer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed)
{
  if (tick_time->tm_min % 15 == 0 || s_minutes_to_launch < 10)
  {
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);
    dict_write_uint8(iter, 0, 0);
    app_message_outbox_send();
  }
  else if (s_minutes_to_launch > 0 && !first_update)
  {
    s_minutes_to_launch--;
    int hours = s_minutes_to_launch / 60;
    int minutes = s_minutes_to_launch % 60;
    snprintf(s_countdown_buffer, sizeof(s_countdown_buffer), "T-%02d:%02d", hours, minutes);
    text_layer_set_text(s_countdown_layer, s_countdown_buffer);
  }
  first_update = false;
  update_time();
}

static void main_window_load(Window *window)
{
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Draw the rocket image
  s_rocket_bitmap = gbitmap_create_with_resource(RESOURCE_ID_ROCKET);
  s_rocket_layer = bitmap_layer_create(
      GRect(0, PBL_IF_ROUND_ELSE(55, 50), bounds.size.w, 60));
  bitmap_layer_set_bitmap(s_rocket_layer, s_rocket_bitmap);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_rocket_layer));

  // Draw the main clock
  s_time_layer = text_layer_create(
      GRect(0, PBL_IF_ROUND_ELSE(10, 0), bounds.size.w, 50));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));

  // Draw the launch countdown
  s_countdown_layer = text_layer_create(
      GRect(0, PBL_IF_ROUND_ELSE(110, 110), bounds.size.w, 50));
  text_layer_set_background_color(s_countdown_layer, GColorClear);
  text_layer_set_text_color(s_countdown_layer, GColorWhite);
  text_layer_set_font(s_countdown_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
  text_layer_set_text_alignment(s_countdown_layer, GTextAlignmentCenter);
  text_layer_set_text(s_countdown_layer, "T-00:00");
  layer_add_child(window_layer, text_layer_get_layer(s_countdown_layer));

  // Draw the rocket name
  s_name_layer = text_layer_create(
      GRect(0, PBL_IF_ROUND_ELSE(130, 135), bounds.size.w, 50));
  text_layer_set_background_color(s_name_layer, GColorClear);
  text_layer_set_text_color(s_name_layer, GColorWhite);
  text_layer_set_font(s_name_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_name_layer, GTextAlignmentCenter);
  text_layer_set_text(s_name_layer, "Loading...");
  layer_add_child(window_layer, text_layer_get_layer(s_name_layer));
}

static void main_window_unload(Window *window)
{
  text_layer_destroy(s_name_layer);
  text_layer_destroy(s_countdown_layer);
  text_layer_destroy(s_time_layer);
  gbitmap_destroy(s_rocket_bitmap);
  bitmap_layer_destroy(s_rocket_layer);
}

static void init()
{
  s_main_window = window_create();

  window_set_window_handlers(s_main_window, (WindowHandlers){
                                                .load = main_window_load,
                                                .unload = main_window_unload});

  window_stack_push(s_main_window, true);
  window_set_background_color(s_main_window, GColorBlack);

  update_time();
  first_update = true;
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);

  const int inbox_size = 256;
  const int outbox_size = 128;
  app_message_open(inbox_size, outbox_size);
}

static void deinit()
{
  window_destroy(s_main_window);
}

int main(void)
{
  init();
  app_event_loop();
  deinit();
}