from flask import Flask, render_template
from influxdb_client import InfluxDBClient
from sklearn.linear_model import LinearRegression
import numpy as np
import time
from threading import Thread

# Initialize Flask application
app = Flask(__name__)

# Function to query temperature data from InfluxDB


def query_temperature():
    # Connect to InfluxDB
    influxdb_client = InfluxDBClient(url="https://iot-group7-service1.iotcloudserve.net",
                                     token="muhb5tqVfkwuHd-0jKZ9SyoYBEkzSQGPMjPZCIGhLS8x5EFq-cUw5nq1kg4DTaRolCJbwV1Ap-K88EFQJK0_oA==", org="Chulalongkorn University")
    query_api = influxdb_client.query_api()

    # Define the query for temperature
    query = '''from(bucket: "RaspberryPi")
    |> range(start: -2h, stop: now())
    |> filter(fn: (r) => r["_measurement"] == "sensor_data")
    |> filter(fn: (r) => r["_field"] == "Temp")
    |> aggregateWindow(every: 5s, fn: mean, createEmpty: false)
    |> yield(name: "mean")'''

    # Execute the query
    result = query_api.query(org="Chulalongkorn University", query=query)

    # Extract temperature values
    temperature_data = []
    for table in result:
        for record in table.records:
            value = record.get_value()
            temperature_data.append(value)

    return temperature_data

# Function to train a simple linear regression model using all but the last data point


def train_linear_regression(temperature_data):
    # Convert to numpy arrays
    temperature_data = np.array(temperature_data).reshape(-1, 1)

    # Split data into features (X) and target (y)
    X = temperature_data[:-1]  # All but the last temperature value
    # All but the first temperature value (shifted by one)
    y = temperature_data[1:]

    # Training the model
    model = LinearRegression()
    model.fit(X, y)

    return model

# Prediction function to forecast one step ahead


def predict_one_step_ahead(model, latest_temperature):
    # Predict the temperature one step ahead
    prediction = model.predict([[latest_temperature]])
    return prediction[0][0]

# Function to write the next temperature prediction to InfluxDB


def write_prediction_to_influxdb(prediction):
    # Connect to InfluxDB
    influxdb_client = InfluxDBClient(url="https://iot-group7-service1.iotcloudserve.net",
                                     token="muhb5tqVfkwuHd-0jKZ9SyoYBEkzSQGPMjPZCIGhLS8x5EFq-cUw5nq1kg4DTaRolCJbwV1Ap-K88EFQJK0_oA==", org="Chulalongkorn University")
    write_api = influxdb_client.write_api()

    # Define the point to write
    point = {
        "measurement": "predicted_temperature",
        "fields": {
            "value": prediction
        }
    }

    # Write the point
    write_api.write(bucket="RaspberryPi", record=point)


def predict_and_publish():
    while True:
        try:
            # Query temperature data
            temperature_data = query_temperature()

            # Train linear regression model
            model = train_linear_regression(temperature_data)

            # Get the latest temperature value
            latest_temperature = temperature_data[-1]

            # Predict one step ahead
            next_temperature = predict_one_step_ahead(
                model, latest_temperature)

            # Write prediction to InfluxDB
            write_prediction_to_influxdb(next_temperature)

            print("Next temperature prediction:", next_temperature)

            # Sleep for a certain interval before making the next prediction
            time.sleep(4)  # Sleep for 60 seconds (adjust as needed)
        except Exception as e:
            print("An error occurred:", e)


# Start a new thread to run the predict_and_publish function continuously
prediction_thread = Thread(target=predict_and_publish)
prediction_thread.daemon = True  # Terminate the thread when the main program exits
prediction_thread.start()

# Run the Flask application
if __name__ == '__main__':
    # Query temperature data
    temperature_data = query_temperature()

    # Train linear regression model
    model = train_linear_regression(temperature_data)

    # Get the latest temperature value
    latest_temperature = temperature_data[-1]

    # Predict one step ahead
    next_temperature = predict_one_step_ahead(model, latest_temperature)

    # Write prediction to InfluxDB
    write_prediction_to_influxdb(next_temperature)

    print("Next temperature prediction:", next_temperature)
    app.run(debug=True, port=5001)
