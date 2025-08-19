# FM Player Analysis Dashboard

A comprehensive Streamlit-based web application designed to import, analyze, and visualize player data from Football Manager. This tool helps you make informed decisions about player roles, tactical suitability, and squad depth by calculating a unique "Dynamic Weighted Role Score" (DWRS) for every player.

## Key Features

*   **HTML Data Import**: Easily upload your squad's HTML export from Football Manager to populate the dashboard.
*   **Dynamic Role Scoring (DWRS)**: Calculates a normalized percentage score for each player in every possible role, based on a weighted average of their attributes.
*   **Customizable Weighting System**: Fine-tune the attribute weights and role multipliers via a simple `config.ini` file to match your tactical preferences.
*   **Role Assignment & Management**: View all players, filter for those without assigned roles, and automatically or manually assign suitable roles based on their positions.
*   **In-Depth Role Analysis**: Select a specific role (e.g., Ball-Playing Defender) and see a ranked list of the best players for that role from your club, your second team, and your scouted list.
*   **Player-Role Matrix**: A powerful grid view showing the DWRS for all your players across every assigned role. Filter by tactic to see only the most relevant roles.
*   **Best Position Calculator**: Select a tactic and automatically generate your best Starting XI and B-Team based on the highest DWRS for each position.
*   **Player Comparison**: Select multiple players for a side-by-side comparison of their complete attribute profiles.
*   **DWRS Progress Tracking**: Monitor player development over time with line charts showing their historical DWRS in specific roles.

## Project Structure

The project is organized to separate different aspects of the application's logic, making it easier to maintain and extend.

```
your-project-folder/
├── config/
│   ├── config.ini          # User-configurable weights and multipliers.
│   └── definitions.json    # Core definitions for roles, tactics, and mappings.
├── databases/
│   └── *.db                # SQLite database files (ignored by git).
├── src/
│   ├── app.py              # Main Streamlit application, handles UI and pages.
│   ├── analytics.py        # Core logic for the DWRS calculation.
│   ├── data_parser.py      # Handles HTML parsing and data processing.
│   ├── sqlite_db.py        # Manages all database interactions.
│   ├── config_handler.py   # Helper functions to read/write config.ini.
│   ├── constants.py        # Loads and centralizes definitions from JSON.
│   └── ...                 # Other source files.
└── README.md               # This file.
```

## Installation

To run this application locally, follow these steps:

1.  **Clone the repository (when you upload it to GitHub):**
    ```bash
    git clone https://github.com/your-username/your-repo-name.git
    cd your-repo-name
    ```

2.  **Create and activate a virtual environment:**
    ```bash
    # For Windows
    python -m venv venv
    .\venv\Scripts\activate

    # For macOS/Linux
    python3 -m venv venv
    source venv/bin/activate
    ```

3.  **Install the required Python packages:**
    *Create a `requirements.txt` file with the following content:*
    ```
    streamlit
    pandas
    beautifulsoup4
    ```
    *Then run the installation command:*
    ```bash
    pip install -r requirements.txt
    ```

## How to Use

1.  **Run the Streamlit Application:**
    Navigate to the `src/` directory and run the following command in your terminal:
    ```bash
    streamlit run app.py
    ```
    Your web browser should automatically open with the application running.

2.  **Initial Setup:**
    *   The application will automatically create a `config/` and `databases/` directory in the project root if they don't exist.
    *   Use the sidebar to upload your first player HTML file exported from Football Manager. The data will be processed and saved automatically.
    *   Go to the "Settings" page or select "Your Club" in the sidebar to configure the dashboard for your team.

3.  **Explore the Pages:**
    *   **Assign Roles**: Start here to automatically assign roles to your players based on their natural positions.
    *   **Role Analysis**: Dive deep into specific roles to find the best-suited players.
    *   **Best Position Calculator**: See your strongest lineup for any of the pre-defined tactics.

## Configuration

You can customize the logic of the DWRS calculation without touching the code:

*   **`config/config.ini`**:
    *   `[Weights]`: Change the importance of global attribute categories (e.g., make "Pace" more or less critical).
    *   `[GKWeights]`: Adjust weights specifically for goalkeepers.
    *   `[RoleMultipliers]`: Increase or decrease the bonus given to "key" and "preferable" attributes for a specific role.

*   **`config/definitions.json`**:
    *   `"role_specific_weights"`: Define which attributes are "key" and "preferable" for each role.
    *   `"tactic_roles"`: Define your own custom tactics by mapping positions to specific roles.
    *   `"tactic_layouts"`: Define the visual layout for your custom tactics on the "Best Position Calculator" page.