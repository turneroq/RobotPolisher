void draw_battery()
{
    //TODO: add inner rects!
    int x1, y1;
    x1 = 15;
    y1 = 8;
  //print the border
  screen.setColor(255, 255, 255);
  screen.drawRect(x1, y1, 4*x1, 3*y1);
  //print inner rects
  //left rect
  //screen.setColor(0, 0, 255);
  //screen.fillRect(11, 11, 16, 19);
  //mid rect
  //screen.setColor(0, 0, 255);
  //screen.fillRect(17, 11, 22, 19);
}

void draw_line_under_header()
{
    screen.setColor(255, 255, 255);
    int x2 = 15;//must be equal battery border x1
    int y2 = 35;
    int y1 = 27;
    //left side
    screen.drawLine(0, y1, x2, y2);

    //mid side. draw under header line
    screen.drawLine(x2, y2, 320-x2, y2);

    //left side

    screen.drawLine(320-x2, y2, 320, y1);    
}

void draw_network_columns()
{
    //TODO: ADD ifs when needs to draw all, half, no columns
    screen.setColor(255, 255, 255);
    int x1 = 15;//must be equal battery border x1
    int y1 = 8; //must be equal battery border y1
    int padding = 5; // from battery border
    int x1_lowest = 4 * x1 + padding; // x
    int y1_lowest = 2.5 * y1;
    int width = 5;
    //int k = 3;//multiplier of battery y2 MUST BE EQUAL
    //draw lowest
    screen.fillRect(x1_lowest, y1_lowest, x1_lowest + width, 3 * y1);

    int x1_middle = x1_lowest + width + padding-2;
    int y1_middle = 2 * y1;
    //draw middle
    screen.fillRect(x1_middle, y1_middle, x1_middle + width, 3 * y1);

    int x1_highest = x1_middle + width + padding-2;
    int y1_highest = 1.5 * y1-2;
    screen.fillRect(x1_highest, y1_highest, x1_highest + width, 3 * y1);
    //draw highest
   // screen.fillRect();
}