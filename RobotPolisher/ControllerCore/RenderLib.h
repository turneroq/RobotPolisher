#define MAX_X_SIZE 320
#define MAX_Y_SIZE 240

struct _header_
{
  int x_up_left_corner_battery;
  int y_up_left_corner_battery;
  int padding_line_from_battery;
  int padding_net_from_battery;
  int net_column_width;
  int padding_between_columns;
  int padding_time_from_right;
} header;

//MUST BE CALLED FIRST
void header_properties_setup()
{
  header.x_up_left_corner_battery = 15;
  header.y_up_left_corner_battery = 8;
  header.padding_line_from_battery = 10;
  header.padding_net_from_battery = 10;
  header.net_column_width = 5;
  header.padding_between_columns = 5;
  header.padding_time_from_right = 130;
}


void draw_battery()
{

  screen.setColor(255, 255, 255);
  screen.drawRect(header.x_up_left_corner_battery,
                  header.y_up_left_corner_battery,
                  4 * header.x_up_left_corner_battery,
                  3 * header.y_up_left_corner_battery);

  int checker_data = battery_checker();
  switch(checker_data)
  {
    case 3:
    {
      // fully charged
      screen.fillRect(header.x_up_left_corner_battery,
                      header.y_up_left_corner_battery,
                      4 * header.x_up_left_corner_battery,
                      3 * header.y_up_left_corner_battery);
      break;
    }
    case 2:
    {
      // 2/3 charged
      screen.fillRect(header.x_up_left_corner_battery,
                      header.y_up_left_corner_battery,
                      3 * header.x_up_left_corner_battery,
                      3 * header.y_up_left_corner_battery);
      break;
    }
    case 1:
    {
      // 1/3 charged
      screen.fillRect(header.x_up_left_corner_battery,
                      header.y_up_left_corner_battery,
                      2 * header.x_up_left_corner_battery,
                      3 * header.y_up_left_corner_battery);
      break;
    }
    case 0:
    {
      // very close to critical minimum charge
      screen.fillRect(header.x_up_left_corner_battery,
                      header.y_up_left_corner_battery,
                      1 * header.x_up_left_corner_battery,
                      3 * header.y_up_left_corner_battery);
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
    int x2 = header.x_up_left_corner_battery;
    int y2 = 3 * header.y_up_left_corner_battery + header.padding_line_from_battery;
    int y1 = 3 * header.y_up_left_corner_battery;

    //left side
    screen.drawLine(0, y1, x2, y2);
    //mid side. draw under header line
    screen.drawLine(x2, y2, MAX_X_SIZE - x2, y2);
    //right side
    screen.drawLine(MAX_X_SIZE - x2, y2, MAX_X_SIZE, y1);    
}

void draw_network_columns()
{
    //TODO: ADD ifs when needs to draw all, half, no columns
    screen.setColor(255, 255, 255);
    int x1 = header.x_up_left_corner_battery;//must be equal battery border x1
    int y1 = header.y_up_left_corner_battery; //must be equal battery border y1
    int padding = header.padding_net_from_battery; // from battery border
    int padding_between = header.padding_between_columns;

    int x1_lowest = 4 * x1 + padding; // x
    int y1_lowest = 2.5 * y1;
    int width = header.net_column_width;
    //int k = 3;//multiplier of battery y2 MUST BE EQUAL
    //draw lowest
    screen.fillRect(x1_lowest, y1_lowest, x1_lowest + width, 3 * y1);

    int x1_middle = x1_lowest + width + padding_between-2;
    int y1_middle = 2 * y1;
    //draw middle
    screen.fillRect(x1_middle, y1_middle, x1_middle + width, 3 * y1);

    int x1_highest = x1_middle + width + padding_between-2;
    int y1_highest = 1.5 * y1-2;
    screen.fillRect(x1_highest, y1_highest, x1_highest + width, 3 * y1);
    //draw highest
   // screen.fillRect();
}

void draw_time_elapsed()
{
  // format: Hrs:Min:Sec
  String time_print = "";
  unsigned long time = millis() / 1000;
  unsigned long time_hrs = time / 3600;
  unsigned long time_min = (time - time_hrs * 3600) / 60;
  unsigned long time_sec = time - time_hrs * 3600 - time_min * 60;
  Serial.print("Hours = ");
  Serial.println(time_hrs);
  Serial.print("Mins = ");
  Serial.println(time_min);
  Serial.print("Seconds = ");
  Serial.println(time_sec);
  char buf[3] = {0};
  if(time_hrs)
  {
    sprintf(buf,"%lu", time_hrs);
    time_print += buf;
    time_print += ':';
  }
  if(time_min)
  {
    if(time_min < 10)
      time_print += '0';
    sprintf(buf,"%lu", time_min);
    time_print += buf;
    time_print += ':';
  }
  else
  {
    time_print += "00:";
  }
  if(time_sec < 10)
    time_print += '0';
  time_print += time_sec;
  screen.print(time_print, MAX_X_SIZE - header.padding_time_from_right, header.y_up_left_corner_battery+5);
}