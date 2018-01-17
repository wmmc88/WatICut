#include <vector>
#include <iostream>
#include <fstream>
#include <cmath>
#include <iomanip>

using namespace std;

#include <SFML/Graphics.hpp>

const double PIX_PER_IN = 100;

//Coded By Walter
void addPoint(int x, int y, sf::RenderWindow & window, 
vector< vector<int> > & coords)
{
	//add x and y to the coords vector
	coords.at(0).push_back(x);
	coords.at(1).push_back(y);
	
	//clear the window are redraw all the circles
	window.clear();
	for(int index = 1; index < coords[0].size(); index++)
	{
		if(coords.at(0).at(index) != -1){
			sf::CircleShape shape(3/16.0 * PIX_PER_IN /2);
        	shape.setFillColor(sf::Color::White);
        	shape.setPosition(coords.at(0).at(index), coords.at(1).at(index));
			window.draw(shape);
		}
	}	
	
	window.display();
}

//Coded by Walter
void toGCode(vector< vector<int> > const & coords, 
			 vector< vector<float> > & path)
{
	//go through every point in the coords vector
	for(int index = 1; index < coords.at(0).size(); index++)
	{
		//has the shape ended
		if(coords.at(0).at(index) != -1)
		{
			//calculates changes in distances and stores them in the path vector
			float dx = coords.at(0).at(index) -  coords.at(0).at(index - 1);
			float dy = coords.at(1).at(index) -  coords.at(1).at(index - 1);
			
			path.at(0).push_back(dx);
			path.at(1).push_back(dy);
			
			//caculates speeds and stores them in the path vector
			//the ratio of dx/dTot ensure that the x and y parts will arrive
			//at the next coordinate at the same time
			float speedX = dx/sqrt(dx*dx + dy*dy) * 100;
			float speedY = dy/sqrt(dx*dx + dy*dy) * 100;
			
			path.at(2).push_back(speedX);
			path.at(3).push_back(speedY);
			
		}
		
		else
		{
			//add (-1,-1,0,0) to signify a spindle lift
			path.at(0).push_back(-1);
			path.at(1).push_back(-1);
			path.at(2).push_back(0);
			path.at(3).push_back(0);
			
			//do the same as above, just with the point before and after
			//the (-1,-1) point
			if(index + 1 < coords.at(0).size()){
			
				index++;
				
				float dx = coords.at(0).at(index) -  coords.at(0).at(index - 2);
				float dy = coords.at(1).at(index) -  coords.at(1).at(index - 2);
			
				path.at(0).push_back(dx);
				path.at(1).push_back(dy);
			
				float speedX = dx/sqrt(dx*dx + dy*dy) * 100;
				float speedY = dy/sqrt(dx*dx + dy*dy) * 100;
			
				path.at(2).push_back(speedX);
				path.at(3).push_back(speedY);
			}	
		}
	}
}

//Coded by Melvin
void toFile(ofstream & fout, vector< vector<float> > const & path)
{
	//outputs the path vector in a way that is convenient for the RobotC program
	fout << fixed << setprecision(2);
	
	for(int index = 0; index < path.at(0).size(); index++)
	{
		fout << (int)-path.at(2).at(index) << " " << (int)path.at(3).at(index)
			 << " " << fabs(path.at(0).at(index))
			 << " " << fabs(path.at(1).at(index)) << endl;
	}
	
	fout << 0 << " " << 0 << " " << -2 << " " << -2;
}

//Coded by Walter
void menu(sf::RenderWindow & window)
{
	sf::Font font;
	if (!font.loadFromFile("arial.ttf"))
	{
    	cout << "Could not find font!";
    	return;
	}
	
	//outputs the instructions to the screen
	sf::Text text;
	text.setFont(font);
	text.setString("Press any key when you are ready to draw your design!\n              (Click and drag to draw your design)");
	text.setFillColor(sf::Color::Magenta);
	text.setStyle(sf::Text::Regular);
	text.setCharacterSize(25);
	text.setPosition(200,500-12.5); 
	window.draw(text);
	window.display();
	
	//waits for the user to type any key
	while (window.isOpen())
    {
        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::KeyPressed)
				return;
		}
	}
}

//Coded by Walter
void drawing(sf::RenderWindow & window, vector< vector<int> > & coords)
{
	//creates a new coords vector
	coords.clear();
	coords.push_back(vector<int>(1,0));
    coords.push_back(vector<int>(1,0));
    window.clear();
    window.display();
	bool pressed = false;
	
	//keep looping while the window is open
	while (window.isOpen())
    {
        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
				window.close();
                
            else if(event.type == sf::Event::MouseButtonPressed)
            	pressed = true;
            	
            else if(event.type == sf::Event::MouseButtonReleased)
            {
            	//if the mouse is released, add (-1,-1) to coords to signify
            	//to signify the end of a shape
            	pressed = false;
            	coords.at(0).push_back(-1);
            	coords.at(1).push_back(-1);
			}
            	
            else if(event.type == sf::Event::MouseMoved && pressed)
            {	
            	//call the addPoint function where the mouse currently is
            	addPoint(event.mouseMove.x, event.mouseMove.y, window, coords);
			}
			
			else if(event.type == sf::Event::KeyPressed)
				//ends the function if the key is pressed
				return;
        }
        
	}
}

//Coded by Gabriel
bool check(sf::RenderWindow & window)
{
	sf::Font font;
	if (!font.loadFromFile("arial.ttf"))
	{
    	cout << "Could not find font!";
    	return true;
	}
	
	//ask the user if they are happy with their design
	sf::Text text;
	text.setFont(font);
	text.setString("Are you satisfied with this design? (Y - Yes/N - Restart)");
	text.setFillColor(sf::Color::Magenta);
	text.setStyle(sf::Text::Regular);
	text.setCharacterSize(25);
	text.setPosition(200,30); 
	window.draw(text);
	window.display();
	
	while (window.isOpen())
    {
        sf::Event event;
        while (window.pollEvent(event))
        {
        	//if they type 'Y' return false, else if they type 'N' return false
            if (event.type == sf::Event::KeyPressed)
				if(sf::Keyboard::isKeyPressed(sf::Keyboard::Y))
					return true;
				else if(sf::Keyboard::isKeyPressed(sf::Keyboard::N))
					return false;
		}
	}
}

//coded by Walter
int main()
{
	//set up the RenderWindow to draw things on
    sf::RenderWindow window(sf::VideoMode(10 * PIX_PER_IN, 10 * PIX_PER_IN),
	"Draw your Design");
    window.setMouseCursorGrabbed(true);
    
    // initialize the coords vector and the ouput file
    window.display();
    vector< vector<int> > coords;
    
	ofstream fout("output.txt");
    
    //call the menu function
    menu(window);
    
    //call the drawing function over and over until the use is satisfied
    do
	{
		drawing(window, coords);
	}while(!check(window));
    
	//initialize the path vector	
	vector< vector<float> > path;
    path.push_back(vector<float>());
    path.push_back(vector<float>());
	path.push_back(vector<float>());
	path.push_back(vector<float>());
	
	//convert the coords to code the robot will read
	toGCode(coords, path);

	//output path to a file to send to the NXT
	toFile(fout, path);
	
    return EXIT_SUCCESS;
}

