#include <pebble.h>

static Window *s_main_window;
static TextLayer *s_hour_layer;
static TextLayer *s_minute_layer;
static TextLayer *s_countdown_layer;
static TextLayer *s_name_layer;
static TextLayer *s_weather_layer;
static BitmapLayer *s_rocket_layer;
static GBitmap *s_rocket_bitmap;
static GFont s_silkscreen_large;
static GFont s_silkscreen_medium;
static GFont s_silkscreen_small;

static char s_countdown_buffer[64];
static char s_rocket_buffer[128];
static char s_weather_buffer[128];

static int32_t s_minutes_to_launch = 0;
static bool first_update;
static bool weather_enabled = false;
static char *weather_units = "";

static void request_weather_data()
{
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_cstring(iter, MESSAGE_KEY_MESSAGE_TYPE, "weather");
  app_message_outbox_send();
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context)
{
  Tuple *countdown_tuple = dict_find(iterator, MESSAGE_KEY_MINUTES_TO_LAUNCH);
  Tuple *rocket_tuple = dict_find(iterator, MESSAGE_KEY_ROCKET);
  Tuple *temperature_tuple = dict_find(iterator, MESSAGE_KEY_TEMPERATURE);
  Tuple *weather_units_tuple = dict_find(iterator, MESSAGE_KEY_UNITS);

  if (countdown_tuple)
  {
    s_minutes_to_launch = countdown_tuple->value->int32;
    int hours = s_minutes_to_launch / 60;
    int minutes = s_minutes_to_launch % 60;
    snprintf(s_countdown_buffer, sizeof(s_countdown_buffer), "T-%02d:%02d", hours, minutes);
    if (s_minutes_to_launch > 0)
      text_layer_set_text(s_countdown_layer, s_countdown_buffer);
    else
      text_layer_set_text(s_countdown_layer, "Lift Off");
  }

  if (rocket_tuple)
  {
    snprintf(s_rocket_buffer, sizeof(s_rocket_buffer), "%s", rocket_tuple->value->cstring);
    text_layer_set_text(s_name_layer, s_rocket_buffer);
  }

  if (weather_units_tuple)
  {
    if (strcmp(weather_units_tuple->value->cstring, weather_units) != 0)
    {
      weather_units = strncpy(weather_units, weather_units_tuple->value->cstring, sizeof(weather_units) - 1);
      weather_units[sizeof(weather_units) - 1] = '\0';
      request_weather_data();
    }
  }

  if (temperature_tuple)
  {
    if (!weather_enabled)
    {
      weather_enabled = true;
      layer_set_hidden(text_layer_get_layer(s_weather_layer), false);
      Layer *window_layer = window_get_root_layer(s_main_window);
    }
    int temperature = temperature_tuple->value->int32;
    snprintf(s_weather_buffer, sizeof(s_weather_buffer), "%d°%s", temperature, weather_units);
    text_layer_set_text(s_weather_layer, s_weather_buffer);
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
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ? "%H %M" : "%I %M", tick_time);

  static char s_hour_buffer[4];
  strftime(s_hour_buffer, sizeof(s_hour_buffer), clock_is_24h_style() ? "%H" : "%I", tick_time);

  static char s_minute_buffer[4];
  strftime(s_minute_buffer, sizeof(s_minute_buffer), "%M", tick_time);

  text_layer_set_text(s_hour_layer, s_hour_buffer);
  text_layer_set_text(s_minute_layer, s_minute_buffer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed)
{
  if (tick_time->tm_min % 60 == 0 && weather_enabled)
  {
    request_weather_data();
  }
  if (tick_time->tm_min % 15 == 0 || s_minutes_to_launch < 10)
  {
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);
    dict_write_cstring(iter, MESSAGE_KEY_MESSAGE_TYPE, "rocket");
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

  int16_t rocket_width;
  int16_t rocket_height;
  if (PBL_PLATFORM_TYPE_CURRENT == PlatformTypeEmery)
  {
    s_rocket_bitmap = gbitmap_create_with_resource(RESOURCE_ID_ROCKET_LARGE);
    rocket_width = 15;
    rocket_height = 30;
  }
  else
  {
    s_rocket_bitmap = gbitmap_create_with_resource(RESOURCE_ID_ROCKET);
    rocket_width = 10;
    rocket_height = 20;
  }
  int16_t time_height = PBL_PLATFORM_TYPE_CURRENT == PlatformTypeEmery ? 58 : 42;
  s_rocket_layer = bitmap_layer_create(
      GRect((bounds.size.w / 2) - (rocket_width / 2), (bounds.size.h / 2) - (rocket_height * 3 / 4) + 2, rocket_width, rocket_height));
  bitmap_layer_set_bitmap(s_rocket_layer, s_rocket_bitmap);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_rocket_layer));

  // Draw the main clock
  int16_t time_padding = PBL_PLATFORM_TYPE_CURRENT == PlatformTypeEmery ? 10 : 8;
  s_hour_layer = text_layer_create(
      GRect(0, (bounds.size.h / 2) - (time_height * 3 / 4), bounds.size.w / 2 - time_padding, time_height));
  text_layer_set_background_color(s_hour_layer, GColorClear);
  text_layer_set_text_color(s_hour_layer, GColorWhite);
  text_layer_set_font(s_hour_layer, s_silkscreen_large);
  text_layer_set_text_alignment(s_hour_layer, GTextAlignmentRight);
  layer_add_child(window_layer, text_layer_get_layer(s_hour_layer));

  s_minute_layer = text_layer_create(
      GRect(bounds.size.w / 2 + time_padding, (bounds.size.h / 2) - (time_height * 3 / 4), bounds.size.w / 2 - time_padding, time_height));
  text_layer_set_background_color(s_minute_layer, GColorClear);
  text_layer_set_text_color(s_minute_layer, GColorWhite);
  text_layer_set_font(s_minute_layer, s_silkscreen_large);
  text_layer_set_text_alignment(s_minute_layer, GTextAlignmentLeft);
  layer_add_child(window_layer, text_layer_get_layer(s_minute_layer));

  int16_t name_height = PBL_PLATFORM_TYPE_CURRENT == PlatformTypeEmery ? 24 : 18;
  int16_t countdown_height = PBL_PLATFORM_TYPE_CURRENT == PlatformTypeEmery ? 28 : 24;

  // Draw the launch countdown
  s_countdown_layer = text_layer_create(
      GRect(0, PBL_IF_ROUND_ELSE(110, bounds.size.h - (name_height + countdown_height + 5)), bounds.size.w, countdown_height));
  text_layer_set_background_color(s_countdown_layer, GColorClear);
  text_layer_set_text_color(s_countdown_layer, GColorWhite);
  text_layer_set_font(s_countdown_layer, s_silkscreen_medium);
  text_layer_set_text_alignment(s_countdown_layer, GTextAlignmentCenter);
  text_layer_set_text(s_countdown_layer, "T-00 00");
  layer_add_child(window_layer, text_layer_get_layer(s_countdown_layer));

  // Draw the rocket name
  s_name_layer = text_layer_create(
      GRect(
          PBL_IF_ROUND_ELSE(bounds.size.w * 0.15, 0),
          PBL_IF_ROUND_ELSE(130, bounds.size.h - (name_height + 5)),
          PBL_IF_ROUND_ELSE(bounds.size.w * 0.7, bounds.size.w),
          name_height));
  text_layer_set_background_color(s_name_layer, GColorClear);
  text_layer_set_text_color(s_name_layer, GColorWhite);
  text_layer_set_font(s_name_layer, s_silkscreen_small);
  text_layer_set_text_alignment(s_name_layer, GTextAlignmentCenter);
  text_layer_set_text(s_name_layer, "Loading...");
  layer_add_child(window_layer, text_layer_get_layer(s_name_layer));

  // Draw the temperature
  int16_t weather_margin_left = PBL_PLATFORM_TYPE_CURRENT == PlatformTypeEmery ? 10 : 5;
  int16_t weather_margin_bottom = PBL_PLATFORM_TYPE_CURRENT == PlatformTypeEmery ? 8 : 2;
  s_weather_layer = text_layer_create(
      GRect(
          0,
          0,
          bounds.size.w,
          30));
  text_layer_set_background_color(s_weather_layer, GColorClear);
  text_layer_set_text_color(s_weather_layer, GColorWhite);
  text_layer_set_font(s_weather_layer, s_silkscreen_small);
  text_layer_set_text_alignment(s_weather_layer, GTextAlignmentCenter);
  text_layer_set_text(s_weather_layer, "");
  layer_add_child(window_layer, text_layer_get_layer(s_weather_layer));

  if (!weather_enabled && false)
  {
    layer_set_hidden(text_layer_get_layer(s_weather_layer), true);
  }
}

static void main_window_unload(Window *window)
{
  text_layer_destroy(s_name_layer);
  text_layer_destroy(s_countdown_layer);
  text_layer_destroy(s_hour_layer);
  text_layer_destroy(s_minute_layer);
  text_layer_destroy(s_weather_layer);
  gbitmap_destroy(s_rocket_bitmap);
  bitmap_layer_destroy(s_rocket_layer);
  fonts_unload_custom_font(s_silkscreen_large);
  fonts_unload_custom_font(s_silkscreen_medium);
  fonts_unload_custom_font(s_silkscreen_small);
}

static void init()
{
  s_main_window = window_create();
  // Check if platform is emery

  if (PBL_PLATFORM_TYPE_CURRENT == PlatformTypeEmery)
  {
    s_silkscreen_large = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SILKSCREEN_58));
    s_silkscreen_medium = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SILKSCREEN_28));
    s_silkscreen_small = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SILKSCREEN_24));
  }
  else
  {
    s_silkscreen_large = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SILKSCREEN_42));
    s_silkscreen_medium = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SILKSCREEN_24));
    s_silkscreen_small = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SILKSCREEN_18));
  }

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