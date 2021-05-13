extern function printf void( string, float);
// ... printf is a variable argument list function, but this would go beyond the scope of this tutorial

class Employee
{
	string name;
	int age;
	float salary;

	function setSalary void( float salary_)
	{
		salary = salary_;
	}
}

function salarySum float( Employee[10] list)
{
	var int idx = 0;
	var float sum = 0.0;
	while (list[idx].age)
	{
		list[idx].setSalary( list[idx].salary * 1.10); // Give them all a 10% raise
		sum = sum + list[idx].salary;
		idx = idx + 1;
	}
	return sum;
}

main
{
	var Employee[ 10] list = {
		{"John Doe", 36, 60000},
		{"Suzy Sample", 36, 63400},
		{"Doe Joe", 36, 64400},
		{"Sandra Last", 36, 67400}
	};
	printf( "Salary sum: %.4f\n", salarySum( list));
}

