#ifndef RENDERLIB_H
#define RENDERLIB_H
#define MAX_X_SIZE 320
#define MAX_Y_SIZE 240

struct _header_
{
  uint16_t x_up_left_corner_battery;
  uint16_t y_up_left_corner_battery;
  uint16_t padding_line_from_battery;
  uint16_t padding_net_from_battery;
  uint16_t net_column_width;
  uint16_t padding_between_columns;
  uint16_t padding_time_from_right;
  uint16_t padding_text_from_line;
  uint16_t polish_flag_radius;
  uint16_t padding_circle_from_line;
  uint16_t padding_between_words;
  uint16_t padding_control_circle_first_letter;
} header;

//MUST BE CALLED FIRST
void header_properties_setup()
{
  //240*320 Screen config
  header.x_up_left_corner_battery = 15u;
  header.y_up_left_corner_battery = 8u;
  header.padding_line_from_battery = 10u;
  header.padding_net_from_battery = 10u;
  header.net_column_width = 5u;
  header.padding_between_columns = 5u;
  header.padding_time_from_right = 130u;
  header.padding_text_from_line = 30u;
  header.polish_flag_radius = 10u;
  header.padding_circle_from_line = 50u;
  header.padding_between_words = 20u;
  header.padding_control_circle_first_letter = 72u;
  //screen.setBackColor(0, 0, 0);// background color for printed objects
}


void draw_battery()
{

  screen.setColor(255, 255, 255);
  screen.drawRect(header.x_up_left_corner_battery,
                  header.y_up_left_corner_battery,
                  4u * header.x_up_left_corner_battery,
                  3u * header.y_up_left_corner_battery);

  uint16_t checker_data = battery_checker();
  switch(checker_data)
  {
    case 3u:
    {
      // fully charged
      screen.fillRect(header.x_up_left_corner_battery,
                      header.y_up_left_corner_battery,
                      4 * header.x_up_left_corner_battery,
                      3 * header.y_up_left_corner_battery);
      break;
    }
    case 2u:
    {
      // 2/3 charged
      screen.fillRect(header.x_up_left_corner_battery,
                      header.y_up_left_corner_battery,
                      3u * header.x_up_left_corner_battery,
                      3u * header.y_up_left_corner_battery);
      break;
    }
    case 1u:
    {
      // 1/3 charged
      screen.fillRect(header.x_up_left_corner_battery,
                      header.y_up_left_corner_battery,
                      2u * header.x_up_left_corner_battery,
                      3u * header.y_up_left_corner_battery);
      break;
    }
    case 0u:
    {
      // very close to critical minimum charge
      screen.fillRect(header.x_up_left_corner_battery,
                      header.y_up_left_corner_battery,
                      1u * header.x_up_left_corner_battery,
                      3u * header.y_up_left_corner_battery);
      break;
    }
    default:
    {
      //all errors go here
      break;
    }
  }
}


void draw_line_under_header()
{
    screen.setColor(255, 255, 255);
    uint16_t x2 = header.x_up_left_corner_battery;
    uint16_t y2 = 3 * header.y_up_left_corner_battery + header.padding_line_from_battery;
    uint16_t y1 = 3 * header.y_up_left_corner_battery;

    //left side
    screen.drawLine(0, y1, x2, y2);
    //mid side. draw under header line
    screen.drawLine(x2, y2, MAX_X_SIZE - x2, y2);
    //right side
    screen.drawLine(MAX_X_SIZE - x2, y2, MAX_X_SIZE, y1);    
}


void draw_network_columns()
{
    screen.setColor(255, 255, 255);
    uint16_t x1 = header.x_up_left_corner_battery;//must be equal battery border x1
    uint16_t y1 = header.y_up_left_corner_battery; //must be equal battery border y1
    uint16_t padding = header.padding_net_from_battery; // from battery border
    uint16_t padding_between = header.padding_between_columns;

    uint16_t x1_lowest = 4u * x1 + padding; // x
    uint16_t y1_lowest = 2.5 * y1;
    uint16_t width = header.net_column_width;
    //int k = 3;//multiplier of battery y2 MUST BE EQUAL
    //draw lowest
    //screen.fillRect(x1_lowest, y1_lowest, x1_lowest + width, 3 * y1);

    uint16_t x1_middle = x1_lowest + width + padding_between-2u;
    uint16_t y1_middle = 2u * y1;
    //draw middle
    //screen.fillRect(x1_middle, y1_middle, x1_middle + width, 3 * y1);

    uint16_t x1_highest = x1_middle + width + padding_between-2u;
    uint16_t y1_highest = 1.5 * y1-2u;
    //draw highest
    //screen.fillRect(x1_highest, y1_highest, x1_highest + width, 3 * y1);

    if(cache.network_drawing_flag == best)
    {
      //draw lowest
      screen.fillRect(x1_lowest, y1_lowest, x1_lowest + width, 3u * y1);
      //draw middle
      screen.fillRect(x1_middle, y1_middle, x1_middle + width, 3u * y1);
      //draw highest
      screen.fillRect(x1_highest, y1_highest, x1_highest + width, 3u * y1);
    }
    else if(cache.network_drawing_flag == medium)
    {
      //draw lowest
      screen.fillRect(x1_lowest, y1_lowest, x1_lowest + width, 3u * y1);
      //draw middle
      screen.fillRect(x1_middle, y1_middle, x1_middle + width, 3u * y1);
    }
    else if(cache.network_drawing_flag == bad)
    {
      //draw lowest
      screen.fillRect(x1_lowest, y1_lowest, x1_lowest + width, 3u * y1);
    }
    else if(cache.network_drawing_flag == timeout)
    {
      //draw lowest
      screen.fillRect(x1_lowest, y1_lowest, x1_lowest + width, 3u * y1);
      //draw middle
      screen.fillRect(x1_middle, y1_middle, x1_middle + width, 3u * y1);
      //draw highest
      screen.fillRect(x1_highest, y1_highest, x1_highest + width, 3u * y1);
      screen.setColor(255, 0, 0);
      //Draw crossed network bars like X
      screen.drawLine(x1_lowest, y1_highest, x1_highest + width, y1_lowest); // part "\"
      screen.drawLine(x1_lowest, y1_lowest, x1_highest + width, y1_highest); // part "/"
      screen.setColor(255, 255, 255); 
    }
}


void draw_time_elapsed()
{
  // format: Hrs:Min:Sec
  String time_print = "";
  uint32_t time = millis() / 1000u;
  uint32_t time_hrs = time / 3600u;
  uint32_t time_min = (time - time_hrs * 3600u) / 60u;
  uint32_t time_sec = time - time_hrs * 3600u - time_min * 60u;
  #if DEBUG_SERIAL
  Serial.print("Hours = ");
  Serial.println(time_hrs);
  Serial.print("Mins = ");
  Serial.println(time_min);
  Serial.print("Seconds = ");
  Serial.println(time_sec);
  #endif
  char buf[3] = {0};
  if(time_hrs)
  {
    sprintf(buf,"%lu", time_hrs);
    time_print += buf;
    time_print += ':';
  }
  if(time_min)
  {
    if(time_min < 10u)
      time_print += '0';
    sprintf(buf,"%lu", time_min);
    time_print += buf;
    time_print += ':';
  }
  else
  {
    time_print += "00:";
  }
  if(time_sec < 10u)
    time_print += '0';
  time_print += time_sec;
  screen.print(time_print, MAX_X_SIZE - header.padding_time_from_right, header.y_up_left_corner_battery+5);
}


void draw_flags()
{
  //drawing if polishing is on or off
  String str1 = "POLISHER";
  screen.print(str1,
        header.x_up_left_corner_battery,
        header.y_up_left_corner_battery + header.padding_line_from_battery + header.padding_text_from_line );
  screen.drawCircle(
    4u * header.x_up_left_corner_battery + header.padding_net_from_battery,
    3u * header.y_up_left_corner_battery + header.padding_line_from_battery + header.padding_circle_from_line,
    header.polish_flag_radius);
    if(state_polish_control == HIGH)
    {
      screen.setColor(0, 255, 0);
      screen.fillCircle(
                        4u * header.x_up_left_corner_battery + header.padding_net_from_battery,
                        3u * header.y_up_left_corner_battery + header.padding_line_from_battery + header.padding_circle_from_line,
                        header.polish_flag_radius - 1u);
    }
    else//LOW
    {
      screen.setColor(255, 0, 0);
      screen.fillCircle(
                        4u * header.x_up_left_corner_battery + header.padding_net_from_battery,
                        3u * header.y_up_left_corner_battery + header.padding_line_from_battery + header.padding_circle_from_line,
                        header.polish_flag_radius - 1u);
    }
  screen.setColor(255, 255, 255);
  //drawing if auto control is on or off
  String str2 = "AUTOMATIC";
  uint16_t x_first_letter = 2u * (4u * header.x_up_left_corner_battery + header.padding_net_from_battery) + header.padding_between_words;
  screen.print(str2,
        x_first_letter,
        header.y_up_left_corner_battery + header.padding_line_from_battery + header.padding_text_from_line );
  screen.drawCircle(
    x_first_letter + header.padding_control_circle_first_letter,
    3u * header.y_up_left_corner_battery + header.padding_line_from_battery + header.padding_circle_from_line,
    header.polish_flag_radius);
    
    if(state_auto_control == HIGH)
    {
      screen.setColor(0, 255, 0);
      screen.fillCircle(
                        x_first_letter + header.padding_control_circle_first_letter,
                        3u * header.y_up_left_corner_battery + header.padding_line_from_battery + header.padding_circle_from_line,
                        header.polish_flag_radius-1u);
    }
    else//LOW
    {
      screen.setColor(255, 0, 0);
      screen.fillCircle(
                        x_first_letter + header.padding_control_circle_first_letter,
                        3u * header.y_up_left_corner_battery + header.padding_line_from_battery + 50u,
                        header.polish_flag_radius-1u);
    }
    screen.setColor(255, 255, 255);
}
#endif