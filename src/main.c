#include "pebble.h"
#include "marquee_text.h"


	
static Window *window;

static TextLayer *temperature_layer;
static MarqueeTextLayer *city_layer;
static BitmapLayer *icon_layer;
static GBitmap *icon_bitmap = NULL;

static AppTimer *marquee_timer;

static AppSync sync;
static uint8_t sync_buffer[64];

enum WeatherKey {
  WEATHER_ICON_KEY = 0x0,         // TUPLE_INT
  WEATHER_TEMPERATURE_KEY = 0x1,  // TUPLE_CSTRING
  WEATHER_CITY_KEY = 0x2,         // TUPLE_CSTRING
};

static const uint32_t WEATHER_ICONS[] = {
  RESOURCE_ID_IMAGE_SUN, //0
  RESOURCE_ID_IMAGE_CLOUD, //1
  RESOURCE_ID_IMAGE_RAIN, //2
  RESOURCE_ID_IMAGE_SNOW //3
};

static void sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Sync Error: %d", app_message_error);
}

static void sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Sync sync_tuple_changed_callback");
  
	switch (key) {
    case WEATHER_ICON_KEY:
		APP_LOG(APP_LOG_LEVEL_DEBUG, "WEATHER_ICON_KEY");
      if (icon_bitmap) {
        gbitmap_destroy(icon_bitmap);
      }
      icon_bitmap = gbitmap_create_with_resource(WEATHER_ICONS[new_tuple->value->uint8]);
      bitmap_layer_set_bitmap(icon_layer, icon_bitmap);
      break;

    case WEATHER_TEMPERATURE_KEY:
		APP_LOG(APP_LOG_LEVEL_DEBUG, "WEATHER_TEMPERATURE_KEY");
		APP_LOG(APP_LOG_LEVEL_DEBUG, new_tuple->value->cstring);
      // App Sync keeps new_tuple in sync_buffer, so we may use it directly
      text_layer_set_text(temperature_layer, new_tuple->value->cstring);
      break;

    case WEATHER_CITY_KEY:
		APP_LOG(APP_LOG_LEVEL_DEBUG, "WEATHER_CITY_KEY");
		APP_LOG(APP_LOG_LEVEL_DEBUG, new_tuple->value->cstring);
      marquee_text_layer_set_text(city_layer, new_tuple->value->cstring);
      break;
  }
}

static void send_cmd(void) {
  Tuplet value = TupletInteger(1, 1);

  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);

  if (iter == NULL) {
    return;
  }

  dict_write_tuplet(iter, &value);
  dict_write_end(iter);

  app_message_outbox_send();
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);

  icon_layer = bitmap_layer_create(GRect(32, 10, 80, 80));
  layer_add_child(window_layer, bitmap_layer_get_layer(icon_layer));

  temperature_layer = text_layer_create(GRect(0, 95, 144, 68));
  text_layer_set_text_color(temperature_layer, GColorWhite);
  text_layer_set_background_color(temperature_layer, GColorClear);
  text_layer_set_font(temperature_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(temperature_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(temperature_layer));

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Creating marquee text layer..");
  city_layer = marquee_text_layer_create(GRect(0, 125, 144, 68));  //city_layer is a pointer to MarqueeTextLayer
	APP_LOG(APP_LOG_LEVEL_DEBUG, "city_layer at %p", city_layer);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Setting marquee text color..");
	marquee_text_layer_set_text_color(city_layer, GColorWhite);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Setting marquee bg color..");
	marquee_text_layer_set_background_color(city_layer, GColorClear);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Setting marquee font..");
	marquee_text_layer_set_font(city_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  //marquee_text_layer_set_text_alignment(city_layer, GTextAlignmentCenter);	
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Adding marquee to window layer..");
	layer_add_child(window_layer, city_layer);
	
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done marquee setup");
	
  //city_layer = text_layer_create(GRect(0, 125, 144, 68));
  //text_layer_set_text_color(city_layer, GColorWhite);
  //text_layer_set_background_color(city_layer, GColorClear);
  //text_layer_set_font(city_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  //text_layer_set_text_alignment(city_layer, GTextAlignmentCenter);
  //layer_add_child(window_layer, text_layer_get_layer(city_layer));

  Tuplet initial_values[] = {
    TupletInteger(WEATHER_ICON_KEY, (uint8_t) 1),
    TupletCString(WEATHER_TEMPERATURE_KEY, "1234C"),
    TupletCString(WEATHER_CITY_KEY, "St Pebblesburg"),
  };

  app_sync_init(&sync, sync_buffer, sizeof(sync_buffer), initial_values, ARRAY_LENGTH(initial_values),
      sync_tuple_changed_callback, sync_error_callback, NULL);

  send_cmd();
}



static void city_marquee_timer_callback(void *data) {
  MarqueeData *marqueedata = layer_get_data(city_layer);
	marqueedata->offset += 1;
	marquee_timer = app_timer_register(50 /* milliseconds */, city_marquee_timer_callback, NULL);
	layer_mark_dirty(city_layer);
}

static void window_unload(Window *window) {
  app_sync_deinit(&sync);

  if (icon_bitmap) {
    gbitmap_destroy(icon_bitmap);
  }

  marquee_text_layer_destroy(city_layer);
  text_layer_destroy(temperature_layer);
  bitmap_layer_destroy(icon_layer);
}

static void init(void) {
  window = window_create();
  window_set_background_color(window, GColorBlack);
  window_set_fullscreen(window, true);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload
  });

  const int inbound_size = 64;
  const int outbound_size = 64;
  app_message_open(inbound_size, outbound_size);

  const bool animated = true;
  window_stack_push(window, animated);
	
	
  // Start the progress timer
  marquee_timer = app_timer_register(50 /* milliseconds */, city_marquee_timer_callback, NULL);

}

static void deinit(void) {
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
